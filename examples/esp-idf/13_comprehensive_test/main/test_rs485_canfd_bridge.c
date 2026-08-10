/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "periph_uart.h"
#include "soc/soc_caps.h"

#include "s31_board_support.h"
#include "s31_test.h"

#define RS485_RX_BUFFER_SIZE 256
#define RS485_RX_TIMEOUT_MS  2000

#define CAN_RX_QUEUE_DEPTH   8
#define CAN_RX_TIMEOUT_MS    2000
#define CAN_BRIDGE_TX_ID     0x123
#define BRIDGE_PAYLOAD_LEN   64

/* The CAN FD <-> RS485 converter is configured with frame-info and frame-ID
 * enabled. Its serial side frame is:
 *   1 byte frame-info + 4 bytes CAN ID + 64 bytes CAN FD payload.
 */
#define CONVERTER_FRAME_INFO 0x2F
#define CONVERTER_HEADER_LEN 5
#define CONVERTER_FRAME_LEN  (CONVERTER_HEADER_LEN + BRIDGE_PAYLOAD_LEN)

typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAIFD_FRAME_MAX_LEN];
} can_rx_item_t;

typedef struct {
    s31_canfd_handle_t *managed;
    twai_node_handle_t node;
    QueueHandle_t rx_queue;
} canfd_ctx_t;

static periph_uart_handle_t *s_rs485_uart;

static bool buffer_contains(const uint8_t *buffer, size_t buffer_len,
                            const uint8_t *needle, size_t needle_len)
{
    if (needle_len == 0 || buffer_len < needle_len) {
        return false;
    }

    for (size_t i = 0; i <= buffer_len - needle_len; i++) {
        if (memcmp(buffer + i, needle, needle_len) == 0) {
            return true;
        }
    }

    return false;
}

static void make_payload(uint8_t payload[BRIDGE_PAYLOAD_LEN], const char *prefix, uint32_t token)
{
    static const char pattern[] = "0123456789ABCDEF";
    size_t prefix_len = strlen(prefix);
    char token_text[9] = {0};

    memset(payload, '0', BRIDGE_PAYLOAD_LEN);
    if (prefix_len > BRIDGE_PAYLOAD_LEN) {
        prefix_len = BRIDGE_PAYLOAD_LEN;
    }
    memcpy(payload, prefix, prefix_len);

    snprintf(token_text, sizeof(token_text), "%08lX", (unsigned long)token);
    if (prefix_len + 8 <= BRIDGE_PAYLOAD_LEN) {
        memcpy(payload + prefix_len, token_text, 8);
    }

    for (size_t i = prefix_len + 8; i < BRIDGE_PAYLOAD_LEN; i++) {
        payload[i] = pattern[(i + token) & 0x0f];
    }
}

/* Build the bytes that must be sent on RS485 when asking the converter to
 * create a CAN FD frame. The CAN ID is sent big-endian, matching the monitor.
 */
static void make_converter_serial_frame(uint8_t frame[CONVERTER_FRAME_LEN],
                                        uint32_t can_id,
                                        const uint8_t payload[BRIDGE_PAYLOAD_LEN])
{
    frame[0] = CONVERTER_FRAME_INFO;
    frame[1] = (uint8_t)(can_id >> 24);
    frame[2] = (uint8_t)(can_id >> 16);
    frame[3] = (uint8_t)(can_id >> 8);
    frame[4] = (uint8_t)can_id;
    memcpy(frame + CONVERTER_HEADER_LEN, payload, BRIDGE_PAYLOAD_LEN);
}

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

static esp_err_t rs485_init(void)
{
    periph_uart_config_t *config = NULL;
    esp_err_t ret = esp_board_periph_ref_handle("uart_rs485", (void **)&s_rs485_uart);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_board_manager_get_periph_config("uart_rs485", (void **)&config);
    if (ret != ESP_OK || !s_rs485_uart || !config) {
        esp_board_periph_unref_handle("uart_rs485");
        s_rs485_uart = NULL;
        return ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(S31_TEST_TAG, "RS485 managed UART%d TX=%d RX=%d RTS/DE=%d baud=%d",
             s_rs485_uart->uart_num, config->tx_io_num, config->rx_io_num,
             config->rts_io_num, config->uart_config.baud_rate);
    ret = uart_flush_input(s_rs485_uart->uart_num);
    if (ret != ESP_OK) {
        esp_board_periph_unref_handle("uart_rs485");
        s_rs485_uart = NULL;
    }
    return ret;
}

static void rs485_deinit(void)
{
    if (s_rs485_uart) {
        esp_err_t ret = esp_board_periph_unref_handle("uart_rs485");
        if (ret != ESP_OK) {
            ESP_LOGW(S31_TEST_TAG, "release uart_rs485 failed: %s", esp_err_to_name(ret));
        }
        s_rs485_uart = NULL;
    }
}

static esp_err_t rs485_send(const uint8_t *payload, size_t payload_len)
{
    ESP_RETURN_ON_FALSE(s_rs485_uart, ESP_ERR_INVALID_STATE, S31_TEST_TAG, "RS485 is not initialized");
    ESP_RETURN_ON_FALSE(payload_len > 0, ESP_ERR_INVALID_ARG, S31_TEST_TAG, "empty RS485 payload");

    int written = uart_write_bytes(s_rs485_uart->uart_num, payload, payload_len);
    if (written != (int)payload_len) {
        return ESP_FAIL;
    }

    esp_err_t ret = uart_wait_tx_done(s_rs485_uart->uart_num, pdMS_TO_TICKS(300));
    vTaskDelay(pdMS_TO_TICKS(10));
    return ret;
}

static esp_err_t rs485_wait_for_payload(const uint8_t *needle, size_t needle_len, int *rx_total_ret)
{
    ESP_RETURN_ON_FALSE(s_rs485_uart, ESP_ERR_INVALID_STATE, S31_TEST_TAG, "RS485 is not initialized");
    uint8_t rx[RS485_RX_BUFFER_SIZE] = {0};
    int rx_total = 0;
    int64_t deadline = esp_timer_get_time() + RS485_RX_TIMEOUT_MS * 1000;

    while (esp_timer_get_time() < deadline && rx_total < (int)sizeof(rx)) {
        int got = uart_read_bytes(s_rs485_uart->uart_num, rx + rx_total,
                                  sizeof(rx) - rx_total, pdMS_TO_TICKS(50));
        if (got > 0) {
            rx_total += got;
            if (buffer_contains(rx, rx_total, needle, needle_len)) {
                if (rx_total_ret) {
                    *rx_total_ret = rx_total;
                }
                return ESP_OK;
            }
        }
    }

    if (rx_total_ret) {
        *rx_total_ret = rx_total;
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t canfd_init(canfd_ctx_t *ctx)
{
    esp_err_t ret = s31_canfd_acquire(&ctx->managed);
    if (ret != ESP_OK) {
        return ret;
    }
    ctx->node = s31_canfd_node(ctx->managed);
    if (!ctx->node) {
        s31_canfd_release();
        ctx->managed = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    ctx->rx_queue = xQueueCreate(CAN_RX_QUEUE_DEPTH, sizeof(can_rx_item_t));
    if (!ctx->rx_queue) {
        s31_canfd_release();
        ctx->managed = NULL;
        ctx->node = NULL;
        return ESP_ERR_NO_MEM;
    }

    const twai_event_callbacks_t callbacks = {
        .on_rx_done = can_on_rx_done,
    };
    ret = twai_node_register_event_callbacks(ctx->node, &callbacks, ctx->rx_queue);
    if (ret == ESP_OK) {
        ret = s31_canfd_enable(ctx->managed);
    }
    if (ret != ESP_OK) {
        s31_canfd_release();
        vQueueDelete(ctx->rx_queue);
        ctx->managed = NULL;
        ctx->node = NULL;
        ctx->rx_queue = NULL;
    }
    return ret;
}

static void canfd_deinit(canfd_ctx_t *ctx)
{
    if (ctx->managed) {
        esp_err_t ret = s31_canfd_release();
        if (ret != ESP_OK) {
            ESP_LOGW(S31_TEST_TAG, "release managed CAN-FD failed: %s", esp_err_to_name(ret));
        }
        ctx->managed = NULL;
        ctx->node = NULL;
    }
    if (ctx->rx_queue) {
        vQueueDelete(ctx->rx_queue);
        ctx->rx_queue = NULL;
    }
}

static esp_err_t canfd_send(canfd_ctx_t *ctx, const uint8_t *payload, size_t payload_len,
                            const twai_frame_header_t *template_header)
{
    ESP_RETURN_ON_FALSE(payload_len > 0 && payload_len <= TWAIFD_FRAME_MAX_LEN,
                        ESP_ERR_INVALID_ARG, S31_TEST_TAG, "invalid CAN FD payload length");

    twai_frame_t frame = {
        .header.id = template_header ? template_header->id : CAN_BRIDGE_TX_ID,
        .header.dlc = twaifd_len2dlc(payload_len),
        .header.ide = template_header ? template_header->ide : false,
        .header.fdf = true,
        /* Default to no BRS because the converter side proved stricter than
         * USB-CANFD tools. When a template is provided, mirror the received bus.
         */
        .header.brs = template_header ? template_header->brs : false,
        .buffer = (uint8_t *)payload,
        .buffer_len = payload_len,
    };

    ESP_LOGI(S31_TEST_TAG, "CANFD TX id=0x%lx len=%u ide=%u brs=%u",
             (unsigned long)frame.header.id,
             (unsigned int)payload_len,
             (unsigned int)frame.header.ide,
             (unsigned int)frame.header.brs);
    ESP_RETURN_ON_ERROR(twai_node_transmit(ctx->node, &frame, 200),
                        S31_TEST_TAG, "CAN FD transmit failed");
    return twai_node_transmit_wait_all_done(ctx->node, 300);
}

static esp_err_t canfd_wait_for_payload(canfd_ctx_t *ctx, const uint8_t *expected, size_t expected_len,
                                        twai_frame_header_t *rx_header_ret, uint16_t *rx_len_ret)
{
    int64_t deadline = esp_timer_get_time() + CAN_RX_TIMEOUT_MS * 1000;

    while (esp_timer_get_time() < deadline) {
        int64_t remain_us = deadline - esp_timer_get_time();
        TickType_t wait_ticks = pdMS_TO_TICKS(remain_us > 50000 ? 50 : (remain_us + 999) / 1000);
        can_rx_item_t rx = {0};

        if (xQueueReceive(ctx->rx_queue, &rx, wait_ticks) != pdTRUE) {
            continue;
        }

        uint16_t frame_len = twaifd_dlc2len(rx.header.dlc);
        if (frame_len > sizeof(rx.data)) {
            frame_len = sizeof(rx.data);
        }
        ESP_LOGI(S31_TEST_TAG, "CANFD RX id=0x%lx len=%u", (unsigned long)rx.header.id, frame_len);

        if (frame_len == expected_len && memcmp(rx.data, expected, expected_len) == 0) {
            if (rx_header_ret) {
                *rx_header_ret = rx.header;
            }
            if (rx_len_ret) {
                *rx_len_ret = frame_len;
            }
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t test_rs485_canfd_bridge(void)
{
#if SOC_TWAI_FD_SUPPORTED
    canfd_ctx_t canfd = {0};
    esp_err_t ret = ESP_OK;
    bool rs485_ready = false;
    uint32_t token = esp_random();
    uint8_t rs485_to_can[BRIDGE_PAYLOAD_LEN] = {0};
    uint8_t can_to_rs485[BRIDGE_PAYLOAD_LEN] = {0};
    uint8_t rs485_to_can_frame[CONVERTER_FRAME_LEN] = {0};
    uint8_t can_to_rs485_frame[CONVERTER_FRAME_LEN] = {0};
    twai_frame_header_t rx_can_header = {0};
    uint16_t rx_can_len = 0;
    int rx_uart_len = 0;

    make_payload(rs485_to_can, "R2C64:", token);
    make_payload(can_to_rs485, "C2R64:", token ^ 0xA5A5A5A5U);
    make_converter_serial_frame(rs485_to_can_frame, CAN_BRIDGE_TX_ID, rs485_to_can);
    make_converter_serial_frame(can_to_rs485_frame, CAN_BRIDGE_TX_ID, can_to_rs485);

    ret = canfd_init(&canfd);
    if (ret != ESP_OK) {
        set_item_result(ITEM_CANFD, TEST_FAIL, "init failed: %s", esp_err_to_name(ret));
        set_item_result(ITEM_RS485, TEST_FAIL, "CANFD init failed");
        return ret;
    }

    ret = rs485_init();
    if (ret != ESP_OK) {
        set_item_result(ITEM_RS485, TEST_FAIL, "init failed: %s", esp_err_to_name(ret));
        set_item_result(ITEM_CANFD, TEST_FAIL, "RS485 init failed");
        canfd_deinit(&canfd);
        return ret;
    }
    rs485_ready = true;

    /* Phase 1: ESP32-S31 sends a CAN FD frame. The converter should output
     * frame-info + CAN ID + payload on RS485, so verify the full 69 bytes.
     */
    uart_flush_input(s_rs485_uart->uart_num);
    set_item_result(ITEM_CANFD, TEST_RUNNING, "tx id=0x%lx 64B", (unsigned long)CAN_BRIDGE_TX_ID);
    set_item_result(ITEM_RS485, TEST_RUNNING, "wait frame 69B");

    ret = canfd_send(&canfd, can_to_rs485, sizeof(can_to_rs485), NULL);
    if (ret != ESP_OK) {
        set_item_result(ITEM_CANFD, TEST_FAIL, "send failed: %s", esp_err_to_name(ret));
        set_item_result(ITEM_RS485, TEST_FAIL, "CANFD send failed");
        goto cleanup;
    }

    ret = rs485_wait_for_payload(can_to_rs485_frame, sizeof(can_to_rs485_frame), &rx_uart_len);
    if (ret != ESP_OK) {
        set_item_result(ITEM_RS485, TEST_FAIL, "no matched 69B frame");
        set_item_result(ITEM_CANFD, TEST_FAIL, "sent, RS485 timeout");
        goto cleanup;
    }

    set_item_result(ITEM_RS485, TEST_PASS, "rx CANFD frame total=%dB", rx_uart_len);

    /* Phase 2: send the same converter serial-frame format back through RS485.
     * The converter should emit one CAN FD frame whose data field is 64 bytes.
     */
    xQueueReset(canfd.rx_queue);
    set_item_result(ITEM_RS485, TEST_RUNNING, "return frame 69B");
    set_item_result(ITEM_CANFD, TEST_RUNNING, "wait RS485 64B");

    ret = rs485_send(rs485_to_can_frame, sizeof(rs485_to_can_frame));
    if (ret != ESP_OK) {
        set_item_result(ITEM_RS485, TEST_FAIL, "return failed: %s", esp_err_to_name(ret));
        set_item_result(ITEM_CANFD, TEST_FAIL, "RS485 return failed");
        goto cleanup;
    }

    ret = canfd_wait_for_payload(&canfd, rs485_to_can, sizeof(rs485_to_can), &rx_can_header, &rx_can_len);
    if (ret != ESP_OK) {
        set_item_result(ITEM_CANFD, TEST_FAIL, "no matched 64B frame");
        set_item_result(ITEM_RS485, TEST_FAIL, "returned, CANFD timeout");
        goto cleanup;
    }

    set_item_result(ITEM_RS485, TEST_PASS, "bridge ok data=64B");
    set_item_result(ITEM_CANFD, TEST_PASS, "bridge ok rx id=0x%lx len=%u",
                    (unsigned long)rx_can_header.id,
                    rx_can_len);

cleanup:
    if (rs485_ready) {
        rs485_deinit();
    }
    canfd_deinit(&canfd);
    return ret;
#else
    set_item_result(ITEM_CANFD, TEST_SKIP, "TWAI-FD unsupported target");
    set_item_result(ITEM_RS485, TEST_SKIP, "CANFD unsupported target");
    return ESP_OK;
#endif
}
