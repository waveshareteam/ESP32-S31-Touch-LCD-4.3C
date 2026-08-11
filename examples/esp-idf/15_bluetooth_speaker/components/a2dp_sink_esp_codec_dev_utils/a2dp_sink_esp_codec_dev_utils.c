/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "a2dp_sink_int_codec_utils.h"
#include "esp_board_manager_includes.h"

#define BT_AV_TAG "BT_AV"
#define AUDIO_SINK_TAG "SNK_CODEC_DEV"

#define RINGBUF_HIGHEST_WATER_LEVEL  (32 * 1024)
#define RINGBUF_PREFETCH_WATER_LEVEL (20 * 1024)
#define AUDIO_SINK_WRITE_CHUNK_BYTES (240 * 6)

#define AUDIO_SINK_DEFAULT_SAMPLE_RATE     (44100)
#define AUDIO_SINK_DEFAULT_BITS_PER_SAMPLE (16)
#define AUDIO_SINK_DEFAULT_CHANNELS        (2)
#define AUDIO_SINK_DEFAULT_VOLUME          (100)

typedef enum {
    RINGBUFFER_MODE_PROCESSING,
    RINGBUFFER_MODE_PREFETCHING,
    RINGBUFFER_MODE_DROPPING,
} audio_sink_ringbuffer_mode_t;

typedef enum {
    CHANNEL_STATUS_IDLE,
    CHANNEL_STATUS_OPENED,
    CHANNEL_STATUS_ENABLED,
} audio_sink_chan_st_t;

typedef struct {
    i2s_chan_handle_t tx_chan;
    esp_codec_dev_handle_t speaker;
    esp_codec_dev_sample_info_t sample_info;
    TaskHandle_t write_task_handle;
    RingbufHandle_t ringbuf;
    SemaphoreHandle_t write_semaphore;
    audio_sink_ringbuffer_mode_t ringbuffer_mode;
    audio_sink_chan_st_t chan_st;
    bool codec_opened;
    bool i2s_enabled;
} audio_sink_srv_codec_dev_cb_t;

static uint32_t s_pkt_cnt;
static audio_sink_srv_codec_dev_cb_t s_audio_sink;
static const char *s_a2d_conn_state_str[] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
static const char *s_a2d_audio_state_str[] = {"Suspended", "Started"};

static esp_err_t audio_sink_codec_to_err(int ret, const char *operation)
{
    if (ret == ESP_CODEC_DEV_OK) {
        return ESP_OK;
    }
    ESP_LOGE(AUDIO_SINK_TAG, "%s failed (%d)", operation, ret);
    return ESP_FAIL;
}

static void audio_sink_sample_info_set(int sample_rate, int channel)
{
    s_audio_sink.sample_info.sample_rate = sample_rate;
    s_audio_sink.sample_info.bits_per_sample = AUDIO_SINK_DEFAULT_BITS_PER_SAMPLE;
    s_audio_sink.sample_info.channel = channel;
    s_audio_sink.sample_info.channel_mask = 0;
}


static esp_err_t audio_sink_i2s_enable(void)
{
    if (s_audio_sink.i2s_enabled) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(s_audio_sink.tx_chan, ESP_ERR_INVALID_STATE,
                        AUDIO_SINK_TAG, "I2S TX channel is not initialized");

    esp_err_t ret = i2s_channel_enable(s_audio_sink.tx_chan);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        s_audio_sink.i2s_enabled = true;
        return ESP_OK;
    }
    ESP_LOGE(AUDIO_SINK_TAG, "enable I2S TX failed (%s)", esp_err_to_name(ret));
    return ret;
}

static esp_err_t audio_sink_i2s_disable(void)
{
    if (!s_audio_sink.i2s_enabled) {
        return ESP_OK;
    }

    esp_err_t ret = i2s_channel_disable(s_audio_sink.tx_chan);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        s_audio_sink.i2s_enabled = false;
        return ESP_OK;
    }
    ESP_LOGE(AUDIO_SINK_TAG, "disable I2S TX failed (%s)", esp_err_to_name(ret));
    return ret;
}

static esp_err_t audio_sink_i2s_init(void)
{
    if (s_audio_sink.tx_chan) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(
        esp_board_manager_get_periph_handle("i2s_audio_out", (void **)&s_audio_sink.tx_chan),
        AUDIO_SINK_TAG, "get Board Manager I2S TX failed");
    ESP_RETURN_ON_FALSE(s_audio_sink.tx_chan, ESP_ERR_INVALID_STATE,
                        AUDIO_SINK_TAG, "Board Manager I2S TX channel is not initialized");
    s_audio_sink.i2s_enabled = true;

    ESP_LOGI(AUDIO_SINK_TAG, "Board Manager I2S TX ready");
    return ESP_OK;
}

static esp_err_t audio_sink_speaker_init(void)
{
    if (s_audio_sink.speaker) {
        return ESP_OK;
    }

    dev_audio_codec_handles_t *dac_handle = NULL;
    ESP_RETURN_ON_ERROR(
        esp_board_manager_get_device_handle("audio_dac", (void **)&dac_handle),
        AUDIO_SINK_TAG, "get Board Manager audio DAC failed");
    ESP_RETURN_ON_FALSE(dac_handle && dac_handle->codec_dev, ESP_ERR_INVALID_STATE,
                        AUDIO_SINK_TAG, "Board Manager audio DAC handle is invalid");

    s_audio_sink.speaker = dac_handle->codec_dev;
    ESP_LOGI(AUDIO_SINK_TAG, "Board Manager ES8389 speaker codec ready");
    return ESP_OK;
}

static esp_err_t audio_sink_reopen_codec(int sample_rate, int channel)
{
    ESP_RETURN_ON_FALSE(s_audio_sink.tx_chan && s_audio_sink.speaker, ESP_ERR_INVALID_STATE,
                        AUDIO_SINK_TAG, "audio sink is not initialized");

    bool was_enabled = s_audio_sink.chan_st == CHANNEL_STATUS_ENABLED;
    if (was_enabled) {
        s_audio_sink.chan_st = CHANNEL_STATUS_OPENED;
    }

    ESP_RETURN_ON_ERROR(audio_sink_i2s_disable(), AUDIO_SINK_TAG, "disable I2S before reconfig failed");

    if (s_audio_sink.codec_opened) {
        ESP_RETURN_ON_ERROR(audio_sink_codec_to_err(esp_codec_dev_close(s_audio_sink.speaker), "close speaker"),
                            AUDIO_SINK_TAG, "close speaker failed");
        s_audio_sink.codec_opened = false;
    }

    audio_sink_sample_info_set(sample_rate, channel);
    ESP_RETURN_ON_ERROR(audio_sink_codec_to_err(esp_codec_dev_open(s_audio_sink.speaker,
                                                                    &s_audio_sink.sample_info),
                                                "open speaker"),
                        AUDIO_SINK_TAG, "open speaker failed");
    s_audio_sink.codec_opened = true;
    s_audio_sink.i2s_enabled = true;

    ESP_RETURN_ON_ERROR(audio_sink_codec_to_err(esp_codec_dev_set_out_vol(s_audio_sink.speaker,
                                                                          AUDIO_SINK_DEFAULT_VOLUME),
                                                "set speaker volume"),
                        AUDIO_SINK_TAG, "set speaker volume failed");

    if (was_enabled) {
        s_audio_sink.chan_st = CHANNEL_STATUS_ENABLED;
    }
    ESP_LOGI(AUDIO_SINK_TAG, "Codec configured: %d Hz, %d channel(s), %d-bit",
             sample_rate, channel, AUDIO_SINK_DEFAULT_BITS_PER_SAMPLE);
    return ESP_OK;
}

static void audio_sink_srv_open(void)
{
    if (s_audio_sink.chan_st != CHANNEL_STATUS_IDLE) {
        ESP_LOGW(AUDIO_SINK_TAG, "Service already open, skipping initialization");
        return;
    }

    audio_sink_sample_info_set(AUDIO_SINK_DEFAULT_SAMPLE_RATE, AUDIO_SINK_DEFAULT_CHANNELS);
    esp_err_t ret = audio_sink_i2s_init();
    if (ret == ESP_OK) {
        ret = audio_sink_speaker_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_SINK_TAG, "audio sink init failed (%s)", esp_err_to_name(ret));
        return;
    }

    s_audio_sink.chan_st = CHANNEL_STATUS_OPENED;
    ret = audio_sink_reopen_codec(s_audio_sink.sample_info.sample_rate,
                                  s_audio_sink.sample_info.channel);
    if (ret != ESP_OK) {
        ESP_LOGE(AUDIO_SINK_TAG, "open codec failed (%s)", esp_err_to_name(ret));
        s_audio_sink.chan_st = CHANNEL_STATUS_IDLE;
    }
}

static void audio_sink_srv_stop(void)
{
    if (s_audio_sink.chan_st == CHANNEL_STATUS_ENABLED) {
        s_audio_sink.chan_st = CHANNEL_STATUS_OPENED;
        (void)audio_sink_i2s_disable();
    }
}

static void audio_sink_srv_close(void)
{
    audio_sink_srv_stop();

    if (s_audio_sink.write_task_handle) {
        vTaskDelete(s_audio_sink.write_task_handle);
        s_audio_sink.write_task_handle = NULL;
    }
    if (s_audio_sink.ringbuf) {
        vRingbufferDelete(s_audio_sink.ringbuf);
        s_audio_sink.ringbuf = NULL;
    }
    if (s_audio_sink.write_semaphore) {
        vSemaphoreDelete(s_audio_sink.write_semaphore);
        s_audio_sink.write_semaphore = NULL;
    }
    if (s_audio_sink.codec_opened) {
        (void)audio_sink_codec_to_err(esp_codec_dev_close(s_audio_sink.speaker), "close speaker");
        s_audio_sink.codec_opened = false;
    }

    s_audio_sink.chan_st = CHANNEL_STATUS_IDLE;
}

static void audio_sink_srv_i2s_task_handler(void *arg)
{
    (void)arg;
    uint8_t *data = NULL;
    size_t item_size = 0;

    for (;;) {
        if (pdTRUE == xSemaphoreTake(s_audio_sink.write_semaphore, portMAX_DELAY)) {
            for (;;) {
                item_size = 0;
                data = (uint8_t *)xRingbufferReceiveUpTo(s_audio_sink.ringbuf,
                                                          &item_size,
                                                          pdMS_TO_TICKS(20),
                                                          AUDIO_SINK_WRITE_CHUNK_BYTES);
                if (item_size == 0) {
                    ESP_LOGI(AUDIO_SINK_TAG, "ringbuffer underflowed, prefetching");
                    s_audio_sink.ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;
                    break;
                }

                if (s_audio_sink.chan_st == CHANNEL_STATUS_ENABLED && s_audio_sink.codec_opened) {
                    (void)audio_sink_codec_to_err(esp_codec_dev_write(s_audio_sink.speaker,
                                                                       data,
                                                                       item_size),
                                                  "write speaker");
                }
                vRingbufferReturnItem(s_audio_sink.ringbuf, data);
            }
        }
    }
}

static void audio_sink_srv_start(void)
{
    if (s_audio_sink.chan_st != CHANNEL_STATUS_OPENED) {
        ESP_LOGE(AUDIO_SINK_TAG, "%s, service wrong state: %d", __func__, s_audio_sink.chan_st);
        return;
    }

    if (!s_audio_sink.codec_opened &&
        audio_sink_reopen_codec(s_audio_sink.sample_info.sample_rate, s_audio_sink.sample_info.channel) != ESP_OK) {
        return;
    }

    if (audio_sink_i2s_enable() != ESP_OK) {
        return;
    }

    ESP_LOGI(AUDIO_SINK_TAG, "ringbuffer empty, prefetching");
    s_audio_sink.ringbuffer_mode = RINGBUFFER_MODE_PREFETCHING;

    if (!s_audio_sink.write_semaphore) {
        s_audio_sink.write_semaphore = xSemaphoreCreateBinary();
        if (!s_audio_sink.write_semaphore) {
            ESP_LOGE(AUDIO_SINK_TAG, "%s, semaphore create failed", __func__);
            goto err_sem;
        }
    }
    if (!s_audio_sink.ringbuf) {
        s_audio_sink.ringbuf = xRingbufferCreate(RINGBUF_HIGHEST_WATER_LEVEL, RINGBUF_TYPE_BYTEBUF);
        if (!s_audio_sink.ringbuf) {
            ESP_LOGE(AUDIO_SINK_TAG, "%s, ringbuffer create failed", __func__);
            goto err_rb;
        }
    }
    if (!s_audio_sink.write_task_handle) {
        if (xTaskCreate(audio_sink_srv_i2s_task_handler, "BtCodecTask", 4 * 1024, NULL,
                        configMAX_PRIORITIES - 3, &s_audio_sink.write_task_handle) != pdPASS) {
            ESP_LOGE(AUDIO_SINK_TAG, "%s, task create failed", __func__);
            goto err_task;
        }
    }

    s_audio_sink.chan_st = CHANNEL_STATUS_ENABLED;
    return;

err_task:
    vRingbufferDelete(s_audio_sink.ringbuf);
    s_audio_sink.ringbuf = NULL;
err_rb:
    vSemaphoreDelete(s_audio_sink.write_semaphore);
    s_audio_sink.write_semaphore = NULL;
err_sem:
    (void)audio_sink_i2s_disable();
}

static void audio_sink_srv_get_codec_info(esp_a2d_mcc_t *mcc, int *sample_rate, int *channel)
{
    *sample_rate = 16000;
    *channel = 2;

    if (mcc->type == ESP_A2D_MCT_SBC) {
        if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_32K) {
            *sample_rate = 32000;
        } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_44K) {
            *sample_rate = 44100;
        } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_48K) {
            *sample_rate = 48000;
        }

        if (mcc->cie.sbc_info.ch_mode & ESP_A2D_SBC_CIE_CH_MODE_MONO) {
            *channel = 1;
        }
    } else if (mcc->type == ESP_A2D_MCT_M24) {
        if (mcc->cie.m24_info.samp_freq2 & ESP_A2D_M24_CIE_SF2_96K) {
            *sample_rate = 96000;
        } else if (mcc->cie.m24_info.samp_freq2 & ESP_A2D_M24_CIE_SF2_88K) {
            *sample_rate = 88200;
        } else if (mcc->cie.m24_info.samp_freq2 & ESP_A2D_M24_CIE_SF2_64K) {
            *sample_rate = 64000;
        } else if (mcc->cie.m24_info.samp_freq2 & ESP_A2D_M24_CIE_SF2_48K) {
            *sample_rate = 48000;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_44K) {
            *sample_rate = 44100;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_32K) {
            *sample_rate = 32000;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_24K) {
            *sample_rate = 24000;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_22K) {
            *sample_rate = 22050;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_16K) {
            *sample_rate = 16000;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_12K) {
            *sample_rate = 12000;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_11K) {
            *sample_rate = 11025;
        } else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_8K) {
            *sample_rate = 8000;
        }

        if (mcc->cie.m24_info.ch & ESP_A2D_M24_CIE_CH_1) {
            *channel = 1;
        }
    }
}

static void audio_sink_srv_codec_info_update(esp_a2d_mcc_t *mcc)
{
    int sample_rate = AUDIO_SINK_DEFAULT_SAMPLE_RATE;
    int channel = AUDIO_SINK_DEFAULT_CHANNELS;

    ESP_LOGI(AUDIO_SINK_TAG, "A2DP audio stream configuration, codec type: %d", mcc->type);
    if (s_audio_sink.chan_st == CHANNEL_STATUS_IDLE) {
        audio_sink_srv_open();
    }
    if (s_audio_sink.chan_st == CHANNEL_STATUS_IDLE) {
        ESP_LOGE(AUDIO_SINK_TAG, "skip codec config because audio sink is not open");
        return;
    }

    audio_sink_srv_get_codec_info(mcc, &sample_rate, &channel);

    if (audio_sink_reopen_codec(sample_rate, channel) != ESP_OK) {
        ESP_LOGE(AUDIO_SINK_TAG, "configure codec failed");
        return;
    }

    if (mcc->type == ESP_A2D_MCT_SBC) {
        ESP_LOGI(AUDIO_SINK_TAG, "Configure audio player: 0x%x-0x%x-0x%x-0x%x-0x%x-%d-%d",
                 mcc->cie.sbc_info.samp_freq,
                 mcc->cie.sbc_info.ch_mode,
                 mcc->cie.sbc_info.block_len,
                 mcc->cie.sbc_info.num_subbands,
                 mcc->cie.sbc_info.alloc_mthd,
                 mcc->cie.sbc_info.min_bitpool,
                 mcc->cie.sbc_info.max_bitpool);
    }
}

static size_t audio_sink_srv_data_output(const uint8_t *data, size_t size)
{
    size_t item_size = 0;

    if (!s_audio_sink.ringbuf) {
        return 0;
    }

    if (s_audio_sink.ringbuffer_mode == RINGBUFFER_MODE_DROPPING) {
        ESP_LOGW(AUDIO_SINK_TAG, "ringbuffer is full, drop this packet");
        vRingbufferGetInfo(s_audio_sink.ringbuf, NULL, NULL, NULL, NULL, &item_size);
        if (item_size <= RINGBUF_PREFETCH_WATER_LEVEL) {
            ESP_LOGI(AUDIO_SINK_TAG, "ringbuffer data decreased, processing");
            s_audio_sink.ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
        }
        return 0;
    }

    BaseType_t done = xRingbufferSend(s_audio_sink.ringbuf, (void *)data, size, 0);
    if (!done) {
        ESP_LOGW(AUDIO_SINK_TAG, "ringbuffer overflowed, dropping");
        s_audio_sink.ringbuffer_mode = RINGBUFFER_MODE_DROPPING;
    }

    if (s_audio_sink.ringbuffer_mode == RINGBUFFER_MODE_PREFETCHING) {
        vRingbufferGetInfo(s_audio_sink.ringbuf, NULL, NULL, NULL, NULL, &item_size);
        if (item_size >= RINGBUF_PREFETCH_WATER_LEVEL) {
            ESP_LOGI(AUDIO_SINK_TAG, "ringbuffer data increased, processing");
            s_audio_sink.ringbuffer_mode = RINGBUFFER_MODE_PROCESSING;
            if (pdFALSE == xSemaphoreGive(s_audio_sink.write_semaphore)) {
                ESP_LOGE(AUDIO_SINK_TAG, "semaphore give failed");
            }
        }
    }

    return done ? size : 0;
}

void bt_a2d_evt_int_codec_hdl(uint16_t event, void *param)
{
    esp_a2d_cb_param_t *a2d = (esp_a2d_cb_param_t *)(param);

    switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT: {
        uint8_t *bda = a2d->conn_stat.remote_bda;
        ESP_LOGI(BT_AV_TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                 s_a2d_conn_state_str[a2d->conn_stat.state],
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            audio_sink_srv_stop();
            audio_sink_srv_close();
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
            esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            audio_sink_srv_start();
        } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING) {
            audio_sink_srv_open();
        }
        break;
    }
    case ESP_A2D_AUDIO_STATE_EVT:
        ESP_LOGI(BT_AV_TAG, "A2DP audio state: %s", s_a2d_audio_state_str[a2d->audio_stat.state]);
        if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
            s_pkt_cnt = 0;
        }
        break;
    case ESP_A2D_AUDIO_CFG_EVT:
        audio_sink_srv_codec_info_update(&a2d->audio_cfg.mcc);
        break;
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

void bt_a2d_data_hdl(const uint8_t *data, uint32_t len)
{
    audio_sink_srv_data_output(data, len);

    if (++s_pkt_cnt % 100 == 0) {
        ESP_LOGI(BT_AV_TAG, "Audio packet count: %" PRIu32, s_pkt_cnt);
    }
}
