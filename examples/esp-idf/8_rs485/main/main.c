#include <inttypes.h>
#include <stdio.h>

#include "driver/uart.h"
#include "esp_board_manager_includes.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "periph_uart.h"

#define RX_BUFFER_SIZE 256

static const char *TAG = "rs485";

void app_main(void)
{
    periph_uart_handle_t *uart = NULL;
    periph_uart_config_t *config = NULL;
    ESP_ERROR_CHECK(esp_board_periph_ref_handle("uart_rs485", (void **)&uart));
    ESP_ERROR_CHECK(esp_board_manager_get_periph_config("uart_rs485", (void **)&config));
    ESP_ERROR_CHECK(uart ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_ERROR_CHECK(config ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "using uart_rs485: UART%d TX=GPIO%d RX=GPIO%d RTS/DE=GPIO%d baud=%d",
             uart->uart_num, config->tx_io_num, config->rx_io_num,
             config->rts_io_num, config->uart_config.baud_rate);

    uint8_t rx[RX_BUFFER_SIZE];
    uint32_t counter = 0;
    while (true) {
        char tx[96];
        int length = snprintf(tx, sizeof(tx), "ESP32-S31 RS485 counter=%" PRIu32 "\r\n", counter++);
        uart_write_bytes(uart->uart_num, tx, length);
        uart_wait_tx_done(uart->uart_num, pdMS_TO_TICKS(100));
        int rx_len = uart_read_bytes(uart->uart_num, rx, sizeof(rx) - 1, pdMS_TO_TICKS(500));
        if (rx_len > 0) {
            rx[rx_len] = 0;
            ESP_LOGI(TAG, "RX %d byte(s): %s", rx_len, (char *)rx);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
