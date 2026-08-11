/*
 * SPDX-FileCopyrightText: 2026 Waveshare Electronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "bt_app_core_utils.h"
#include "bredr_app_common_utils.h"
#include "a2dp_sink_common_utils.h"
#include "a2dp_utils_tags.h"
#include "music_ui.h"
#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
#include "a2dp_sink_int_codec_utils.h"
#else
#include "a2dp_sink_ext_codec_utils.h"
#endif

/* device name */
static const char local_device_name[] = CONFIG_EXAMPLE_LOCAL_DEVICE_NAME;

/* event for stack up */
enum {
    BT_APP_EVT_STACK_UP = 0,
};

/********************************
 * STATIC FUNCTION DECLARATIONS
 *******************************/

/* Device callback function */
static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param);

/* GAP callback function */
static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);

/* callback function for A2DP sink */
static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);

/* handler for AVRCP controller events */
static void bt_app_avrc_ct_evt_hdl(uint16_t event, void *param);

/* callback function for AVRCP controller */
static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param);

#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
/* callback function for A2DP sink audio data stream */
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len);
#else
/* callback function for A2DP sink undecoded audio data */
static void bt_app_a2d_audio_data_cb(esp_a2d_conn_hdl_t conn_hdl, esp_a2d_audio_buff_t *audio_buf);
#endif

/* handler for bluetooth stack enabled events */
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

static uint8_t s_avrc_tl;
static esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;

/*******************************
 * STATIC FUNCTION DEFINITIONS
 ******************************/

static void bt_app_dev_cb(esp_bt_dev_cb_event_t event, esp_bt_dev_cb_param_t *param)
{
    bredr_app_dev_evt_def_hdl(event, param);
}

static void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    bredr_app_gap_evt_def_hdl(event, param);
}

static uint8_t bt_app_avrc_alloc_tl(void)
{
    if (s_avrc_tl > ESP_AVRC_TRANS_LABEL_MAX) {
        s_avrc_tl = 0;
    }
    return s_avrc_tl++;
}

static bool bt_app_avrc_peer_supports(uint8_t event_id)
{
    return esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &s_avrc_peer_rn_cap, event_id);
}

static uint32_t bt_app_avrc_known_time_ms(uint32_t time_ms)
{
    return time_ms == UINT32_MAX ? 0 : time_ms;
}

static void bt_app_avrc_register_notification(uint8_t event_id, uint32_t event_parameter)
{
    if (bt_app_avrc_peer_supports(event_id)) {
        esp_avrc_ct_send_register_notification_cmd(bt_app_avrc_alloc_tl(), event_id, event_parameter);
    }
}

static void bt_app_avrc_request_metadata(void)
{
    const uint8_t attr_mask = ESP_AVRC_MD_ATTR_TITLE |
                              ESP_AVRC_MD_ATTR_ARTIST |
                              ESP_AVRC_MD_ATTR_ALBUM |
                              ESP_AVRC_MD_ATTR_GENRE |
                              ESP_AVRC_MD_ATTR_PLAYING_TIME;

    esp_avrc_ct_send_metadata_cmd(bt_app_avrc_alloc_tl(), attr_mask);
    esp_avrc_ct_send_get_play_status_cmd(bt_app_avrc_alloc_tl());
}

static void bt_app_avrc_copy_metadata(void *p_dest, void *p_src, int len)
{
    (void)len;
    esp_avrc_ct_cb_param_t *dest = (esp_avrc_ct_cb_param_t *)p_dest;
    esp_avrc_ct_cb_param_t *src = (esp_avrc_ct_cb_param_t *)p_src;

    dest->meta_rsp.attr_id = src->meta_rsp.attr_id;
    dest->meta_rsp.attr_length = src->meta_rsp.attr_length;
    dest->meta_rsp.attr_text = NULL;

    if (!src->meta_rsp.attr_text || src->meta_rsp.attr_length <= 0) {
        return;
    }

    dest->meta_rsp.attr_text = malloc(src->meta_rsp.attr_length + 1);
    if (!dest->meta_rsp.attr_text) {
        return;
    }
    memcpy(dest->meta_rsp.attr_text, src->meta_rsp.attr_text, src->meta_rsp.attr_length);
    dest->meta_rsp.attr_text[src->meta_rsp.attr_length] = '\0';
}

static void bt_app_avrc_free_metadata(void *p_param)
{
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)p_param;
    if (rc && rc->meta_rsp.attr_text) {
        free(rc->meta_rsp.attr_text);
        rc->meta_rsp.attr_text = NULL;
    }
}

static void bt_app_ui_update_audio_cfg(const esp_a2d_mcc_t *mcc)
{
    if (!mcc || mcc->type != ESP_A2D_MCT_SBC) {
        return;
    }

    int sample_rate = 16000;
    int channel_count = 2;
    if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_32K) {
        sample_rate = 32000;
    } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_44K) {
        sample_rate = 44100;
    } else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_48K) {
        sample_rate = 48000;
    }

    if (mcc->cie.sbc_info.ch_mode & ESP_A2D_SBC_CIE_CH_MODE_MONO) {
        channel_count = 1;
    }
    music_ui_set_codec_info(sample_rate, channel_count);
}

static void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
    case ESP_A2D_PROF_STATE_EVT:
    case ESP_A2D_SNK_PSC_CFG_EVT:
    case ESP_A2D_SNK_SET_DELAY_VALUE_EVT:
    case ESP_A2D_SNK_GET_DELAY_VALUE_EVT: {
        bt_app_work_dispatch(bt_a2d_evt_def_hdl, event, param, sizeof(esp_a2d_cb_param_t), NULL, NULL);
        break;
    }
    case ESP_A2D_CONNECTION_STATE_EVT:
    case ESP_A2D_AUDIO_STATE_EVT:
    case ESP_A2D_AUDIO_CFG_EVT:
    case ESP_A2D_SEP_REG_STATE_EVT: {
        if (event == ESP_A2D_CONNECTION_STATE_EVT) {
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                music_ui_set_peer_connected(true);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                music_ui_set_peer_connected(false);
            }
        } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
            music_ui_set_stream_state(param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED ? "Started" : "Suspended");
        } else if (event == ESP_A2D_AUDIO_CFG_EVT) {
            bt_app_ui_update_audio_cfg(&param->audio_cfg.mcc);
        }
#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
        bt_app_work_dispatch(bt_a2d_evt_int_codec_hdl, event, param, sizeof(esp_a2d_cb_param_t), NULL, NULL);
#else
        bt_app_work_dispatch(bt_a2d_evt_ext_codec_hdl, event, param, sizeof(esp_a2d_cb_param_t), NULL, NULL);
#endif
        break;
    }
    default:
        ESP_LOGE(BT_AV_TAG, "Invalid A2DP event: %d", event);
        break;
    }
}

#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
static void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    bt_a2d_data_hdl(data, len);
}
#else
static void bt_app_a2d_audio_data_cb(esp_a2d_conn_hdl_t conn_hdl, esp_a2d_audio_buff_t *audio_buf)
{
    bt_a2d_audio_data_hdl(conn_hdl, audio_buf);
}
#endif

static void bt_app_avrc_ct_evt_hdl(uint16_t event, void *param)
{
    esp_avrc_ct_cb_param_t *rc = (esp_avrc_ct_cb_param_t *)param;

    switch (event) {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
        ESP_LOGI(BT_AV_TAG, "AVRCP CT connection: %d", rc->conn_stat.connected);
        // 不要用 AVRCP 的连接状态控制 UI 是否蓝牙连接
        // music_ui_set_peer_connected(rc->conn_stat.connected);
        if (rc->conn_stat.connected) {
            music_ui_set_peer_connected(true);
            esp_avrc_ct_send_get_rn_capabilities_cmd(bt_app_avrc_alloc_tl());
            bt_app_avrc_request_metadata();
        } else {
            s_avrc_peer_rn_cap.bits = 0;
            // 这里不要置 false，避免切歌/歌词切换时闪到 Waiting for phone
        }
        break;
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
        s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;
        bt_app_avrc_register_notification(ESP_AVRC_RN_TRACK_CHANGE, 0);
        bt_app_avrc_register_notification(ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
        bt_app_avrc_register_notification(ESP_AVRC_RN_PLAY_POS_CHANGED, 1);
        bt_app_avrc_request_metadata();
        break;
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
        if (rc->change_ntf.event_id == ESP_AVRC_RN_TRACK_CHANGE) {
            // 不要清空界面
            // music_ui_clear_metadata();
            bt_app_avrc_request_metadata();
            bt_app_avrc_register_notification(ESP_AVRC_RN_TRACK_CHANGE, 0);
        } else if (rc->change_ntf.event_id == ESP_AVRC_RN_PLAY_STATUS_CHANGE) {
            music_ui_set_playback_status(rc->change_ntf.event_parameter.playback);
            bt_app_avrc_register_notification(ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
        } else if (rc->change_ntf.event_id == ESP_AVRC_RN_PLAY_POS_CHANGED) {
            music_ui_set_play_position(bt_app_avrc_known_time_ms(rc->change_ntf.event_parameter.play_pos));
            bt_app_avrc_register_notification(ESP_AVRC_RN_PLAY_POS_CHANGED, 1);
        }
        break;
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        if (rc->meta_rsp.attr_text) {
            music_ui_set_metadata_attr(rc->meta_rsp.attr_id, rc->meta_rsp.attr_text, rc->meta_rsp.attr_length);
            ESP_LOGI(BT_AV_TAG, "AVRCP metadata attr 0x%x: %s", rc->meta_rsp.attr_id, (char *)rc->meta_rsp.attr_text);
        }
        break;
    case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT:
        music_ui_set_playback_status(rc->play_status_rsp.play_status);
        music_ui_set_play_position(bt_app_avrc_known_time_ms(rc->play_status_rsp.song_position));
        music_ui_set_play_duration(bt_app_avrc_known_time_ms(rc->play_status_rsp.song_length));
        break;
    case ESP_AVRC_CT_PROF_STATE_EVT:
        ESP_LOGI(BT_AV_TAG, "AVRCP CT profile state: %d", rc->avrc_ct_init_stat.state);
        break;
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
        break;
    default:
        ESP_LOGW(BT_AV_TAG, "Unhandled AVRCP CT event: %d", event);
        break;
    }
}

static void bt_app_rc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
    case ESP_AVRC_CT_METADATA_RSP_EVT:
        bt_app_work_dispatch(bt_app_avrc_ct_evt_hdl, event, param, sizeof(esp_avrc_ct_cb_param_t),
                             bt_app_avrc_copy_metadata, bt_app_avrc_free_metadata);
        break;
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
    case ESP_AVRC_CT_PLAY_STATUS_RSP_EVT:
    case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
    case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
    case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
    case ESP_AVRC_CT_SET_ABSOLUTE_VOLUME_RSP_EVT:
    case ESP_AVRC_CT_PROF_STATE_EVT:
        bt_app_work_dispatch(bt_app_avrc_ct_evt_hdl, event, param, sizeof(esp_avrc_ct_cb_param_t), NULL, NULL);
        break;
    default:
        ESP_LOGW(BT_AV_TAG, "Invalid AVRCP CT event: %d", event);
        break;
    }
}

static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s event: %d", __func__, event);

    switch (event) {
    /* when do the stack up, this event comes */
    case BT_APP_EVT_STACK_UP: {
        esp_bt_gap_set_device_name(local_device_name);
        esp_bt_dev_register_callback(bt_app_dev_cb);
        esp_bt_gap_register_callback(bt_app_gap_cb);

        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);
        assert(esp_avrc_ct_init() == ESP_OK);

        esp_a2d_register_callback(&bt_app_a2d_cb);
        assert(esp_a2d_sink_init() == ESP_OK);

#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
#else
        esp_a2d_mcc_t mcc = {0};
        mcc.type = ESP_A2D_MCT_SBC;
        mcc.cie.sbc_info.samp_freq = ESP_A2D_SBC_CIE_SF_16K |
                                     ESP_A2D_SBC_CIE_SF_32K |
                                     ESP_A2D_SBC_CIE_SF_44K |
                                     ESP_A2D_SBC_CIE_SF_48K;
        mcc.cie.sbc_info.ch_mode = ESP_A2D_SBC_CIE_CH_MODE_MONO |
                                   ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL |
                                   ESP_A2D_SBC_CIE_CH_MODE_STEREO |
                                   ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO;
        mcc.cie.sbc_info.block_len = ESP_A2D_SBC_CIE_BLOCK_LEN_4 |
                                     ESP_A2D_SBC_CIE_BLOCK_LEN_8 |
                                     ESP_A2D_SBC_CIE_BLOCK_LEN_12 |
                                     ESP_A2D_SBC_CIE_BLOCK_LEN_16;
        mcc.cie.sbc_info.num_subbands = ESP_A2D_SBC_CIE_NUM_SUBBANDS_4 | ESP_A2D_SBC_CIE_NUM_SUBBANDS_8;
        mcc.cie.sbc_info.alloc_mthd = ESP_A2D_SBC_CIE_ALLOC_MTHD_SNR | ESP_A2D_SBC_CIE_ALLOC_MTHD_LOUDNESS;
        mcc.cie.sbc_info.max_bitpool = 250;
        mcc.cie.sbc_info.min_bitpool = 2;
        /* register stream end point, only support SBC currently */
        esp_a2d_sink_register_stream_endpoint(0, &mcc);
        esp_a2d_sink_register_audio_data_callback(bt_app_a2d_audio_data_cb);
#endif

        /* Get the default value of the delay value */
        esp_a2d_sink_get_delay_value();
        /* Get local device name */
        esp_bt_gap_get_device_name();

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        break;
    }
    /* others */
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled event: %d", __func__, event);
        break;
    }
}

/*******************************
 * MAIN ENTRY POINT
 ******************************/

void app_main(void)
{
#if CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC == FALSE
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("audio_dac"));
#endif

    if (music_ui_start() != ESP_OK) {
        ESP_LOGW(BT_AV_TAG, "music UI disabled");
    }

    ESP_ERROR_CHECK(bredr_app_common_init());


    bt_app_task_start_up();
    /* bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL, NULL);

}
