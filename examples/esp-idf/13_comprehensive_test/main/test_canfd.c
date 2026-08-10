/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_check.h"
#include "esp_random.h"
#include "esp_twai.h"
#include "soc/soc_caps.h"

#include "s31_board_support.h"
#include "s31_test.h"

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAIFD_FRAME_MAX_LEN];
} can_rx_item_t;

static bool IRAM_ATTR can_on_rx_done(twai_node_handle_t handle,
                                     const twai_rx_done_event_data_t *edata,
                                     void *user_ctx)
{
    (void)edata;
    BaseType_t task_woken = pdFALSE;
    QueueHandle_t queue = (QueueHandle_t)user_ctx;
    can_rx_item_t item = {0};
    twai_frame_t frame = {
        .buffer = item.data,
        .buffer_len = sizeof(item.data),
    };

    if (twai_node_receive_from_isr(handle, &frame) == ESP_OK) {
        item.header = frame.header;
        xQueueSendFromISR(queue, &item, &task_woken);
    }

    return task_woken == pdTRUE;
}

esp_err_t test_canfd(void)
{
#if SOC_TWAI_FD_SUPPORTED
    s31_canfd_handle_t *managed = NULL;
    esp_err_t ret = s31_canfd_acquire(&managed);
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "acquire managed CAN-FD failed");
    twai_node_handle_t node = s31_canfd_node(managed);
    if (!node) {
        s31_canfd_release();
        return ESP_ERR_INVALID_STATE;
    }

    QueueHandle_t rx_queue = xQueueCreate(4, sizeof(can_rx_item_t));
    if (!rx_queue) {
        s31_canfd_release();
        return ESP_ERR_NO_MEM;
    }

    const twai_event_callbacks_t callbacks = {
        .on_rx_done = can_on_rx_done,
    };
    ret = twai_node_register_event_callbacks(node, &callbacks, rx_queue);
    if (ret == ESP_OK) {
        ret = s31_canfd_enable(managed);
    }

    uint8_t payload[16] = {0};
    memcpy(payload, "S31 CAN FD", 10);
    uint32_t token = esp_random();
    payload[12] = (uint8_t)(token >> 24);
    payload[13] = (uint8_t)(token >> 16);
    payload[14] = (uint8_t)(token >> 8);
    payload[15] = (uint8_t)token;

    if (ret == ESP_OK) {
        twai_frame_t frame = {
            .header.id = 0x123,
            .header.dlc = twaifd_len2dlc(sizeof(payload)),
            .header.fdf = true,
            .header.brs = true,
            .buffer = payload,
            .buffer_len = sizeof(payload),
        };
        ret = twai_node_transmit(node, &frame, 200);
        if (ret == ESP_OK) {
            ret = twai_node_transmit_wait_all_done(node, 200);
        }
    }

    can_rx_item_t rx = {0};
    if (ret == ESP_OK && xQueueReceive(rx_queue, &rx, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ret = ESP_ERR_TIMEOUT;
    }
    if (ret == ESP_OK) {
        uint8_t rx_len = twaifd_dlc2len(rx.header.dlc);
        if (rx.header.id != 0x123 || rx_len != sizeof(payload) ||
            memcmp(rx.data, payload, sizeof(payload)) != 0) {
            ret = ESP_FAIL;
        }
    }

    esp_err_t release_ret = s31_canfd_release();
    vQueueDelete(rx_queue);
    if (ret == ESP_OK) {
        ret = release_ret;
    }
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "CAN FD loopback failed");

    set_item_result(ITEM_CANFD, TEST_PASS, "loopback id=0x123 len=16");
    return ESP_OK;
#else
    set_item_result(ITEM_CANFD, TEST_SKIP, "TWAI-FD unsupported target");
    return ESP_OK;
#endif
}
