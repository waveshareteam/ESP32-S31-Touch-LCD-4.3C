#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "s31_board_support.h"

#include "soc/soc_caps.h"
typedef struct {
    twai_frame_header_t header;
    uint8_t data[TWAIFD_FRAME_MAX_LEN];
} rx_item_t;

static const char *TAG = "canfd";

static bool IRAM_ATTR on_rx_done(twai_node_handle_t node,
                                 const twai_rx_done_event_data_t *event,
                                 void *user_context)
{
    (void)event;
    BaseType_t task_woken = pdFALSE;
    rx_item_t item = {0};
    twai_frame_t frame = {.buffer = item.data, .buffer_len = sizeof(item.data)};
    if (twai_node_receive_from_isr(node, &frame) == ESP_OK) {
        item.header = frame.header;
        xQueueSendFromISR((QueueHandle_t)user_context, &item, &task_woken);
    }
    return task_woken == pdTRUE;
}

static void receive_task(void *arg)
{
    rx_item_t item;
    while (true) {
        xQueueReceive((QueueHandle_t)arg, &item, portMAX_DELAY);
        uint8_t length = twaifd_dlc2len(item.header.dlc);
        ESP_LOGI(TAG, "RX id=0x%03" PRIx32 " len=%u fd=%u brs=%u",
                 item.header.id, length, item.header.fdf, item.header.brs);
        ESP_LOG_BUFFER_HEX(TAG, item.data, length);
    }
}

void app_main(void)
{
#if SOC_TWAI_FD_SUPPORTED
    esp_err_t ret = ESP_OK;
    s31_canfd_handle_t *managed = NULL;
    twai_node_handle_t node = NULL;
    QueueHandle_t queue = NULL;
    TaskHandle_t receive_task_handle = NULL;

    ret = s31_canfd_acquire(&managed);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    node = s31_canfd_node(managed);
    if (!node) {
        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }
    queue = xQueueCreate(8, sizeof(rx_item_t));
    if (!queue) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    const twai_event_callbacks_t callbacks = {.on_rx_done = on_rx_done};
    ret = twai_node_register_event_callbacks(node, &callbacks, queue);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    ret = s31_canfd_enable(managed);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    if (xTaskCreate(receive_task, "canfd_rx", 4096, queue, 10,
                    &receive_task_handle) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    uint32_t counter = 0;
    while (true) {
        uint8_t payload[16] = {0};
        memcpy(payload, "S31 CAN FD", 10);
        payload[12] = (uint8_t)(counter >> 24);
        payload[13] = (uint8_t)(counter >> 16);
        payload[14] = (uint8_t)(counter >> 8);
        payload[15] = (uint8_t)counter;
        twai_frame_t frame = {
            .header.id = 0x123,
            .header.dlc = twaifd_len2dlc(sizeof(payload)),
            .header.fdf = true,
            .header.brs = true,
            .buffer = payload,
            .buffer_len = sizeof(payload),
        };
        ret = twai_node_transmit(node, &frame, pdMS_TO_TICKS(100));
        if (ret == ESP_OK) {
            twai_node_transmit_wait_all_done(node, pdMS_TO_TICKS(100));
            ESP_LOGI(TAG, "TX counter=%" PRIu32, counter++);
        } else {
            ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

cleanup:
    if (receive_task_handle) {
        vTaskDelete(receive_task_handle);
    }
    if (managed) {
        s31_canfd_release();
    }
    if (queue) {
        vQueueDelete(queue);
    }
    ESP_ERROR_CHECK(ret);
#else
    ESP_LOGE(TAG, "This target does not support TWAI-FD");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
}
