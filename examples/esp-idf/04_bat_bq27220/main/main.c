#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board_support.h"

#define BATTERY_RETRY_DELAY_MS 2000

static const char *TAG = "bat_bq27220";

void app_main(void)
{
    while (true) {
        s31_battery_handle_t *battery = NULL;
        esp_err_t ret = s31_battery_acquire(&battery);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG,
                     "BQ27220 is not responding: %s. The gauge is powered from "
                     "the battery; connect a battery and verify the I2C bus with "
                     "01_i2c_scan. Retrying...",
                     esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(BATTERY_RETRY_DELAY_MS));
            continue;
        }

        ESP_LOGI(TAG, "BQ27220 detected");
        while (true) {
            s31_battery_info_t info = {0};
            ret = s31_battery_get_info(battery, &info);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "BQ27220 read failed: %s", esp_err_to_name(ret));
                break;
            }
            ESP_LOGI(TAG,
                     "voltage=%umV current=%dmA avg=%dmA power=%dmW cap=%u/%umAh soc=%u%% soh=%u%% temp=%.1fC cycles=%u max_load=%dmA tte=%umin ttf=%umin status=0x%04x",
                     info.voltage_mv, info.current_ma, info.average_current_ma,
                     info.average_power_mw, info.remaining_capacity_mah,
                     info.full_charge_capacity_mah, info.state_of_charge_percent,
                     info.state_of_health_percent, info.temperature_c,
                     info.cycle_count, info.max_load_ma, info.time_to_empty_min,
                     info.time_to_full_min, info.status);
            vTaskDelay(pdMS_TO_TICKS(BATTERY_RETRY_DELAY_MS));
        }

        ret = s31_battery_release();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "BQ27220 release failed: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(BATTERY_RETRY_DELAY_MS));
    }
}
