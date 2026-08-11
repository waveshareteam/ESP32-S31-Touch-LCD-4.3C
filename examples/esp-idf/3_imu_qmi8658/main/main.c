#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board_support.h"

static const char *TAG = "imu_qmi8658";

void app_main(void)
{
    s31_imu_handle_t *imu = NULL;
    ESP_ERROR_CHECK(s31_imu_acquire(&imu));
    uint8_t who = 0;
    uint8_t revision = 0;
    ESP_ERROR_CHECK(s31_imu_get_identity(imu, &who, &revision));
    ESP_LOGI(TAG, "WHO_AM_I=0x%02x revision=0x%02x", who, revision);

    uint32_t sample = 0;
    while (true) {
        bool ready = false;
        uint8_t status_int = 0;
        uint8_t status0 = 0;
        esp_err_t ret = s31_imu_is_data_ready(imu, &ready, &status_int, &status0);
        if (ret == ESP_OK && ready) {
            s31_imu_data_t data = {0};
            ESP_ERROR_CHECK(s31_imu_read(imu, &data));
            ESP_LOGI(TAG,
                     "sample=%lu acc[g]=%+.3f,%+.3f,%+.3f gyro[dps]=%+.2f,%+.2f,%+.2f temp=%.2fC int=0x%02x status0=0x%02x gpio=%d",
                     (unsigned long)++sample, data.accel_x_g, data.accel_y_g,
                     data.accel_z_g, data.gyro_x_dps, data.gyro_y_dps,
                     data.gyro_z_dps, data.temperature_c, data.status_int,
                     data.status0, data.int_gpio_level);
        } else if (ret != ESP_OK) {
            ESP_LOGW(TAG, "status read failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGD(TAG, "waiting: INT=0x%02x STATUS0=0x%02x", status_int, status0);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
