#include <ctype.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUDIO_BUFFER_BYTES 4096
#define OUTPUT_CHANNELS 2
#define OUTPUT_CHANNEL_MASK 0x3

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
} wav_info_t;

static const char *TAG = "codec_wav";

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool is_wav(const char *name)
{
    size_t len = strlen(name);
    return len >= 4 && name[len - 4] == '.' &&
           tolower((unsigned char)name[len - 3]) == 'w' &&
           tolower((unsigned char)name[len - 2]) == 'a' &&
           tolower((unsigned char)name[len - 1]) == 'v';
}

static esp_err_t find_wav(const char *mount, char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/test.wav", mount);
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
        return ESP_OK;
    }
    DIR *dir = opendir(mount);
    ESP_RETURN_ON_FALSE(dir, ESP_FAIL, TAG, "open %s failed", mount);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (is_wav(entry->d_name)) {
            int len = snprintf(path, path_size, "%s/%s", mount, entry->d_name);
            closedir(dir);
            return len > 0 && (size_t)len < path_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
        }
    }
    closedir(dir);
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t parse_wav(FILE *file, wav_info_t *info)
{
    uint8_t header[12];
    ESP_RETURN_ON_FALSE(fread(header, 1, sizeof(header), file) == sizeof(header),
                        ESP_FAIL, TAG, "RIFF header");
    ESP_RETURN_ON_FALSE(memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0,
                        ESP_ERR_INVALID_ARG, TAG, "not WAVE");
    bool format_found = false;
    while (true) {
        uint8_t chunk[8];
        ESP_RETURN_ON_FALSE(fread(chunk, 1, sizeof(chunk), file) == sizeof(chunk),
                            ESP_FAIL, TAG, "chunk header");
        uint32_t size = read_le32(chunk + 4);
        long start = ftell(file);
        ESP_RETURN_ON_FALSE(start >= 0, ESP_FAIL, TAG, "get chunk offset");
        if (memcmp(chunk, "fmt ", 4) == 0) {
            uint8_t format[16];
            ESP_RETURN_ON_FALSE(size >= sizeof(format) &&
                                fread(format, 1, sizeof(format), file) == sizeof(format),
                                ESP_ERR_INVALID_ARG, TAG, "fmt chunk");
            ESP_RETURN_ON_FALSE(read_le16(format) == 1, ESP_ERR_NOT_SUPPORTED, TAG, "PCM only");
            info->channels = read_le16(format + 2);
            info->sample_rate = read_le32(format + 4);
            info->bits_per_sample = read_le16(format + 14);
            ESP_RETURN_ON_FALSE(info->sample_rate > 0 &&
                                (info->channels == 1 || info->channels == 2) &&
                                info->bits_per_sample == 16,
                                ESP_ERR_NOT_SUPPORTED, TAG, "16-bit mono/stereo only");
            format_found = true;
        } else if (memcmp(chunk, "data", 4) == 0) {
            ESP_RETURN_ON_FALSE(format_found, ESP_ERR_INVALID_ARG, TAG, "data before fmt");
            long data_offset = ftell(file);
            ESP_RETURN_ON_FALSE(data_offset >= 0 && (uint64_t)data_offset <= UINT32_MAX,
                                ESP_ERR_INVALID_SIZE, TAG, "invalid data offset");
            info->data_offset = (uint32_t)data_offset;
            info->data_size = size;
            return ESP_OK;
        }
        ESP_RETURN_ON_FALSE(fseek(file, start + size + (size & 1), SEEK_SET) == 0,
                            ESP_FAIL, TAG, "seek next chunk");
    }
}

static esp_err_t play_wav(esp_codec_dev_handle_t codec, const char *path)
{
    esp_err_t ret = ESP_FAIL;
    bool codec_opened = false;
    FILE *file = fopen(path, "rb");
    ESP_RETURN_ON_FALSE(file, ESP_FAIL, TAG, "open %s failed", path);
    wav_info_t wav = {0};
    uint8_t *input = NULL;
    uint8_t *output = NULL;
    ESP_GOTO_ON_ERROR(parse_wav(file, &wav), cleanup, TAG, "parse WAV");
    ESP_LOGI(TAG, "Playing %s: %" PRIu32 " Hz, %u-bit, %u channel(s), %" PRIu32 " bytes",
             path, wav.sample_rate, (unsigned)wav.bits_per_sample,
             (unsigned)wav.channels, wav.data_size);
    esp_codec_dev_sample_info_t format = {
        .sample_rate = wav.sample_rate,
        .channel = OUTPUT_CHANNELS,
        .channel_mask = OUTPUT_CHANNEL_MASK,
        .bits_per_sample = wav.bits_per_sample,
        .mclk_multiple = 256,
    };
    ESP_GOTO_ON_FALSE(esp_codec_dev_open(codec, &format) == ESP_CODEC_DEV_OK,
                      ESP_FAIL, cleanup, TAG, "open codec");
    codec_opened = true;
    ESP_GOTO_ON_FALSE(esp_codec_dev_set_out_vol(codec, 100) == ESP_CODEC_DEV_OK,
                      ESP_FAIL, cleanup, TAG, "set codec volume");
    input = malloc(AUDIO_BUFFER_BYTES);
    output = malloc(AUDIO_BUFFER_BYTES * 2);
    ESP_GOTO_ON_FALSE(input && output, ESP_ERR_NO_MEM, cleanup, TAG, "audio buffers");

    ESP_GOTO_ON_FALSE(fseek(file, wav.data_offset, SEEK_SET) == 0,
                      ESP_FAIL, cleanup, TAG, "seek audio data");
    uint32_t remaining = wav.data_size;
    while (remaining) {
        size_t want = remaining > AUDIO_BUFFER_BYTES ? AUDIO_BUFFER_BYTES : remaining;
        size_t got = fread(input, 1, want, file);
        ESP_GOTO_ON_FALSE(got == want, ESP_FAIL, cleanup, TAG,
                          "WAV data truncated: wanted %zu bytes, got %zu", want, got);
        remaining -= got;
        void *data = input;
        size_t bytes = got;
        if (wav.channels == 1) {
            ESP_GOTO_ON_FALSE((got % sizeof(int16_t)) == 0, ESP_ERR_INVALID_SIZE,
                              cleanup, TAG, "unaligned mono PCM data");
            size_t count = got / sizeof(int16_t);
            int16_t *mono = (int16_t *)input;
            int16_t *stereo = (int16_t *)output;
            for (size_t i = 0; i < count; i++) {
                stereo[i * 2] = mono[i];
                stereo[i * 2 + 1] = mono[i];
            }
            data = output;
            bytes = got * 2;
        }
        ESP_GOTO_ON_FALSE(esp_codec_dev_write(codec, data, bytes) == ESP_CODEC_DEV_OK,
                          ESP_FAIL, cleanup, TAG, "codec write");
    }
    ret = ESP_OK;

cleanup:
    free(input);
    free(output);
    fclose(file);
    if (codec_opened) {
        int close_ret = esp_codec_dev_close(codec);
        if (ret == ESP_OK && close_ret != ESP_CODEC_DEV_OK) {
            ret = ESP_FAIL;
        }
    }
    return ret;
}

static void deinit_device(const char *name)
{
    esp_err_t ret = esp_board_manager_deinit_device_by_name(name);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to deinitialize %s: %s", name, esp_err_to_name(ret));
    }
}

void app_main(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name("fs_sdcard");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card initialization failed: %s", esp_err_to_name(ret));
        deinit_device("sdcard_power_ctrl");
        return;
    }

    ret = esp_board_manager_init_device_by_name("audio_dac");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio codec initialization failed: %s", esp_err_to_name(ret));
        deinit_device("fs_sdcard");
        return;
    }

    dev_fs_fat_handle_t *sd = NULL;
    dev_audio_codec_handles_t *dac = NULL;
    ret = esp_board_manager_get_device_handle("fs_sdcard", (void **)&sd);
    if (ret == ESP_OK) {
        ret = esp_board_manager_get_device_handle("audio_dac", (void **)&dac);
    }
    if (ret == ESP_OK && (!sd || !sd->mount_point || !dac || !dac->codec_dev)) {
        ret = ESP_ERR_INVALID_STATE;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get initialized device handles: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    char path[256] = {0};
    ret = find_wav(sd->mount_point, path, sizeof(path));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No playable WAV file found: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_LOGI(TAG, "Selected WAV file: %s", path);
    while (true) {
        ret = play_wav(dac->codec_dev, path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Playback failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGI(TAG, "Playback complete, restarting in 1 second");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

cleanup:
    deinit_device("audio_dac");
    deinit_device("fs_sdcard");
}
