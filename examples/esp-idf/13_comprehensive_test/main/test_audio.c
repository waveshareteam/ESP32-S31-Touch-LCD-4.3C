/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_types.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_board_manager_includes.h"
#include "s31_test.h"

#define MIC_SAMPLE_RATE            48000
#define MIC_BITS_PER_SAMPLE        16
#define MIC_CHANNELS               2
#define MIC_FRAME_SAMPLES          256
#define MIC_BUFFER_BYTES           (MIC_FRAME_SAMPLES * MIC_CHANNELS * sizeof(int16_t))
#define MIC_INPUT_GAIN_DB          18.0f
#define MIC_GATE_OPEN_LEVEL        320
#define MIC_GATE_CLOSE_LEVEL       192
#define MIC_LEVEL_GAIN             8
#define MIC_LEVEL_MAX              32767
#define MIC_LEVEL_RELEASE_DIVISOR  8
#define MIC_LEVEL_RELEASE_FLOOR    64
#define MIC_LOG_INTERVAL_FRAMES    20
#define MIC_TASK_DELAY_MS          20
#define SPEAKER_TONE_MS            2000
#define SPEAKER_TONE_HZ            1000
#define SPEAKER_TONE_FADE_MS       50
#define SPEAKER_TONE_AMPLITUDE     10000
#define SPEAKER_TONE_FRAME_SAMPLES 240
#define SPEAKER_TONE_TOTAL_SAMPLES (MIC_SAMPLE_RATE * SPEAKER_TONE_MS / 1000)
#define SPEAKER_TONE_FADE_SAMPLES  (MIC_SAMPLE_RATE * SPEAKER_TONE_FADE_MS / 1000)
#define SPEAKER_SINE_LUT_SIZE      (MIC_SAMPLE_RATE / SPEAKER_TONE_HZ)

static const int16_t s_sine_lut[SPEAKER_SINE_LUT_SIZE] = {
    0, 4277, 8481, 12539, 16384, 19947, 23170, 25996,
    28377, 30273, 31650, 32487, 32767, 32487, 31650, 30273,
    28377, 25996, 23170, 19947, 16384, 12539, 8481, 4277,
    0, -4277, -8481, -12539, -16384, -19947, -23170, -25996,
    -28377, -30273, -31650, -32487, -32767, -32487, -31650, -30273,
    -28377, -25996, -23170, -19947, -16384, -12539, -8481, -4277,
};

typedef struct {
    bool gate_open;
    int32_t level;
} mic_meter_state_t;

static int32_t mic_frame_activity(const int16_t *samples, int channel)
{
    int32_t sum = 0;
    for (int i = 0; i < MIC_FRAME_SAMPLES; i++) {
        sum += samples[i * MIC_CHANNELS + channel];
    }
    int32_t mean = sum / MIC_FRAME_SAMPLES;

    uint32_t deviation_sum = 0;
    for (int i = 0; i < MIC_FRAME_SAMPLES; i++) {
        int32_t delta = (int32_t)samples[i * MIC_CHANNELS + channel] - mean;
        deviation_sum += (uint32_t)(delta < 0 ? -delta : delta);
    }
    return (int32_t)(deviation_sum / MIC_FRAME_SAMPLES);
}

static int32_t mic_meter_update(mic_meter_state_t *meter, int32_t activity)
{
    if (!meter->gate_open) {
        if (activity < MIC_GATE_OPEN_LEVEL) {
            meter->level = 0;
            return 0;
        }
        meter->gate_open = true;
    } else if (activity <= MIC_GATE_CLOSE_LEVEL) {
        meter->gate_open = false;
        meter->level = 0;
        return 0;
    }

    int32_t target = activity >= MIC_GATE_OPEN_LEVEL
                         ? (activity - MIC_GATE_CLOSE_LEVEL) * MIC_LEVEL_GAIN
                         : 0;
    if (target > MIC_LEVEL_MAX) {
        target = MIC_LEVEL_MAX;
    }
    if (target >= meter->level) {
        meter->level = target;
    } else {
        int32_t drop = (meter->level - target + MIC_LEVEL_RELEASE_DIVISOR - 1) /
                       MIC_LEVEL_RELEASE_DIVISOR;
        meter->level -= drop;
    }
    if (target == 0 && meter->level <= MIC_LEVEL_RELEASE_FLOOR) {
        meter->gate_open = false;
        meter->level = 0;
    }
    return meter->level;
}

static void mic_meter_reset(mic_meter_state_t *meter, volatile int32_t *level)
{
    meter->gate_open = false;
    meter->level = 0;
    *level = 0;
}

static int16_t speaker_tone_sample(uint32_t sample_index)
{
    uint32_t gain_q15 = INT16_MAX;
    if (sample_index < SPEAKER_TONE_FADE_SAMPLES) {
        gain_q15 = sample_index * INT16_MAX / SPEAKER_TONE_FADE_SAMPLES;
    } else {
        uint32_t samples_remaining = SPEAKER_TONE_TOTAL_SAMPLES - sample_index - 1;
        if (samples_remaining < SPEAKER_TONE_FADE_SAMPLES) {
            gain_q15 = samples_remaining * INT16_MAX / SPEAKER_TONE_FADE_SAMPLES;
        }
    }

    int32_t sample = (int32_t)s_sine_lut[sample_index % SPEAKER_SINE_LUT_SIZE] *
                     SPEAKER_TONE_AMPLITUDE / INT16_MAX;
    return (int16_t)(sample * (int32_t)gain_q15 / INT16_MAX);
}

static esp_err_t codec_ret_to_err(int ret)
{
    return ret == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}

void test_speaker_play_tone(void)
{
#if CONFIG_S31_TEST_ENABLE_SPEAKER_TONE
    esp_err_t ret = esp_board_manager_init_device_by_name("audio_dac");
    bool dac_initialized = ret == ESP_OK;
    dev_audio_codec_handles_t *dac = NULL;
    if (ret == ESP_OK) {
        ret = esp_board_manager_get_device_handle("audio_dac", (void **)&dac);
    }
    if (ret != ESP_OK || !dac || !dac->codec_dev) {
        set_item_result(ITEM_SPEAKER, TEST_FAIL, "speaker codec init failed");
        if (dac_initialized) {
            esp_board_manager_deinit_device_by_name("audio_dac");
        }
        return;
    }

    bool codec_opened = false;
    int16_t *tone = NULL;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = MIC_SAMPLE_RATE,
        .channel = MIC_CHANNELS,
        .bits_per_sample = MIC_BITS_PER_SAMPLE,
        .channel_mask = 0,
        .mclk_multiple = 256,
    };
    ret = codec_ret_to_err(esp_codec_dev_open(dac->codec_dev, &fs));
    if (ret == ESP_OK) {
        codec_opened = true;
        ret = codec_ret_to_err(esp_codec_dev_set_out_vol(dac->codec_dev, 50));
    }
    if (ret == ESP_OK) {
        tone = heap_caps_malloc(SPEAKER_TONE_FRAME_SAMPLES * MIC_CHANNELS * sizeof(int16_t),
                                MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ret = tone ? ESP_OK : ESP_ERR_NO_MEM;
    }

    int frames = SPEAKER_TONE_TOTAL_SAMPLES / SPEAKER_TONE_FRAME_SAMPLES;
    uint32_t sample_index = 0;
    for (int f = 0; ret == ESP_OK && f < frames; f++) {
        for (int i = 0; i < SPEAKER_TONE_FRAME_SAMPLES; i++) {
            int16_t sample = speaker_tone_sample(sample_index);
            tone[i * 2] = sample;
            tone[i * 2 + 1] = sample;
            sample_index++;
        }
        ret = codec_ret_to_err(esp_codec_dev_write(
            dac->codec_dev, tone,
            SPEAKER_TONE_FRAME_SAMPLES * MIC_CHANNELS * sizeof(int16_t)));
    }

    free(tone);
    if (codec_opened) {
        esp_codec_dev_close(dac->codec_dev);
    }
    esp_err_t release_ret = esp_board_manager_deinit_device_by_name("audio_dac");
    if (ret == ESP_OK) {
        ret = release_ret;
    }
    set_item_result(ITEM_SPEAKER, ret == ESP_OK ? TEST_MANUAL : TEST_FAIL,
                    ret == ESP_OK ? "tone sent, listen" : esp_err_to_name(ret));
#else
    set_item_result(ITEM_SPEAKER, TEST_SKIP, "tone disabled");
#endif
}

void audio_task(void *arg)
{
    (void)arg;
    test_speaker_play_tone();

    esp_err_t ret = esp_board_manager_init_device_by_name("audio_adc");
    bool adc_initialized = ret == ESP_OK;
    dev_audio_codec_handles_t *adc = NULL;
    if (ret == ESP_OK) {
        ret = esp_board_manager_get_device_handle("audio_adc", (void **)&adc);
    }
    if (ret != ESP_OK || !adc || !adc->codec_dev) {
        set_item_result(ITEM_MIC, TEST_FAIL, "mic codec init failed");
        if (adc_initialized) {
            esp_board_manager_deinit_device_by_name("audio_adc");
        }
        vTaskDelete(NULL);
        return;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = MIC_SAMPLE_RATE,
        .channel = MIC_CHANNELS,
        .bits_per_sample = MIC_BITS_PER_SAMPLE,
        .channel_mask = 0,
        .mclk_multiple = 256,
    };
    bool codec_opened = false;
    ret = codec_ret_to_err(esp_codec_dev_open(adc->codec_dev, &fs));
    if (ret == ESP_OK) {
        codec_opened = true;
        ret = codec_ret_to_err(esp_codec_dev_set_in_gain(adc->codec_dev, MIC_INPUT_GAIN_DB));
    }
    int16_t *samples = NULL;
    if (ret == ESP_OK) {
        samples = heap_caps_malloc(MIC_BUFFER_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ret = samples ? ESP_OK : ESP_ERR_NO_MEM;
    }
    if (ret != ESP_OK) {
        set_item_result(ITEM_MIC, TEST_FAIL, "%s", esp_err_to_name(ret));
        if (codec_opened) {
            esp_codec_dev_close(adc->codec_dev);
        }
        esp_board_manager_deinit_device_by_name("audio_adc");
        vTaskDelete(NULL);
        return;
    }

    set_item_result(ITEM_MIC, TEST_MANUAL, "speak and watch level");
    mic_meter_state_t left_meter = {0};
    mic_meter_state_t right_meter = {0};
    uint32_t frame_count = 0;
    s_mic_level_l = 0;
    s_mic_level_r = 0;
    while (true) {
        esp_err_t read_ret = codec_ret_to_err(
            esp_codec_dev_read(adc->codec_dev, samples, MIC_BUFFER_BYTES));
        frame_count++;
        if (read_ret == ESP_OK) {
            int32_t left_activity = mic_frame_activity(samples, 0);
            int32_t right_activity = mic_frame_activity(samples, 1);
            s_mic_level_l = mic_meter_update(&left_meter, left_activity);
            s_mic_level_r = mic_meter_update(&right_meter, right_activity);
            if (frame_count >= MIC_LOG_INTERVAL_FRAMES) {
                ESP_LOGI(S31_TEST_TAG, "MIC activity L=%ld R=%ld, level L=%ld R=%ld",
                         (long)left_activity, (long)right_activity,
                         (long)s_mic_level_l, (long)s_mic_level_r);
            }
        } else {
            mic_meter_reset(&left_meter, &s_mic_level_l);
            mic_meter_reset(&right_meter, &s_mic_level_r);
            if (frame_count >= MIC_LOG_INTERVAL_FRAMES) {
                ESP_LOGW(S31_TEST_TAG, "MIC read failed: %s", esp_err_to_name(read_ret));
            }
        }
        if (frame_count >= MIC_LOG_INTERVAL_FRAMES) {
            frame_count = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(MIC_TASK_DELAY_MS));
    }
}
