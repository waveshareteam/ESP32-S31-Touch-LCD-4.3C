/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/uart.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "periph_uart.h"

#include "s31_test.h"

#define RS485_RX_BUFFER_SIZE 256

esp_err_t test_rs485(void)
{
    periph_uart_handle_t *uart = NULL;
    esp_err_t ret = esp_board_periph_ref_handle("uart_rs485", (void **)&uart);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!uart) {
        esp_board_periph_unref_handle("uart_rs485");
        return ESP_ERR_INVALID_STATE;
    }

    ret = uart_flush_input(uart->uart_num);
    char tx[80] = {0};
    snprintf(tx, sizeof(tx), "ESP32-S31 RS485 self-test %u\r\n", (unsigned int)esp_random());
    int tx_len = (int)strlen(tx);
    uint8_t rx[RS485_RX_BUFFER_SIZE] = {0};
    int rx_total = 0;

    if (ret == ESP_OK) {
        int written = uart_write_bytes(uart->uart_num, tx, tx_len);
        ret = written == tx_len ? ESP_OK : ESP_FAIL;
    }
    if (ret == ESP_OK) {
        ret = uart_wait_tx_done(uart->uart_num, pdMS_TO_TICKS(100));
    }

    int64_t deadline = esp_timer_get_time() + 1000 * 1000;
    while (ret == ESP_OK && esp_timer_get_time() < deadline &&
           rx_total < (int)sizeof(rx) - 1) {
        int got = uart_read_bytes(uart->uart_num, rx + rx_total,
                                  sizeof(rx) - 1 - rx_total, pdMS_TO_TICKS(100));
        if (got > 0) {
            rx_total += got;
            rx[rx_total] = '\0';
            if (strstr((const char *)rx, tx)) {
                break;
            }
        }
    }
    if (ret == ESP_OK && strstr((const char *)rx, tx) == NULL) {
        ret = ESP_ERR_TIMEOUT;
    }

    esp_err_t release_ret = esp_board_periph_unref_handle("uart_rs485");
    if (ret == ESP_OK) {
        ret = release_ret;
    }
    ESP_RETURN_ON_ERROR(ret, S31_TEST_TAG, "RS485 loopback failed");

    set_item_result(ITEM_RS485, TEST_PASS, "loopback %d bytes", rx_total);
    return ESP_OK;
}
