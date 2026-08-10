#include <inttypes.h>
#include <stdlib.h>

#include "esp_board_manager_includes.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define FRAME_SAMPLES 256
#define BUFFER_BYTES (FRAME_SAMPLES * CHANNELS * sizeof(int16_t))

static const char *TAG = "codec_mic_raw";

void app_main(void)
{
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("audio_adc"));
    dev_audio_codec_handles_t *adc = NULL;
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("audio_adc", (void **)&adc));
    ESP_ERROR_CHECK(adc && adc->codec_dev ? ESP_OK : ESP_ERR_INVALID_STATE);

    esp_codec_dev_sample_info_t format = {
        .sample_rate = SAMPLE_RATE,
        .channel = CHANNELS,
        .bits_per_sample = 16,
        .mclk_multiple = 256,
    };
    ESP_ERROR_CHECK(esp_codec_dev_open(adc->codec_dev, &format) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(adc->codec_dev, 30.0f) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL);
    int16_t *samples = heap_caps_malloc(BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(samples ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_LOGI(TAG, "streaming raw stereo microphone samples: %d Hz, 16-bit, %d channels",
             SAMPLE_RATE, CHANNELS);

    uint32_t frame = 0;
    while (true) {
        ESP_ERROR_CHECK(esp_codec_dev_read(adc->codec_dev, samples, BUFFER_BYTES) == ESP_CODEC_DEV_OK
                            ? ESP_OK : ESP_FAIL);
        int32_t left_peak = 0;
        int32_t right_peak = 0;
        for (int i = 0; i < FRAME_SAMPLES; i++) {
            int32_t left = abs(samples[i * 2]);
            int32_t right = abs(samples[i * 2 + 1]);
            left_peak = left > left_peak ? left : left_peak;
            right_peak = right > right_peak ? right : right_peak;
        }
        ESP_LOGI(TAG,
                 "frame=%" PRIu32 " peak L=%ld R=%ld first samples: [%d,%d] [%d,%d] [%d,%d] [%d,%d]",
                 frame++, (long)left_peak, (long)right_peak,
                 samples[0], samples[1], samples[2], samples[3],
                 samples[4], samples[5], samples[6], samples[7]);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
