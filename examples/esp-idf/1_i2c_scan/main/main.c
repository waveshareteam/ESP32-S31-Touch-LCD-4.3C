#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_board_manager_includes.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SCAN_TIMEOUT_MS 50

static const char *TAG = "i2c_scan";
static const char *I2C_MASTER_TAG = "i2c.master";

static const char *known_device_name(uint8_t address)
{
    switch (address) {
    case 0x18:
    case 0x14:
    case 0x19:
    case 0x38:
    case 0x5d:
        return "GT911 touch";
    case 0x6a:
    case 0x6b:
        return "QMI8658 IMU";
    case 0x51:
        return "PCF85063ATL RTC";
    case 0x55:
        return "BQ27220 battery gauge";
    case 0x10:
    case 0x11:
        return "ES8389 codec";
    case 0x24:
        return "board IO expander";
    default:
        return "";
    }
}

static bool i2c_bus_is_idle(const i2c_master_bus_config_t *config)
{
    int sda_level = gpio_get_level(config->sda_io_num);
    int scl_level = gpio_get_level(config->scl_io_num);
    ESP_LOGI(TAG, "bus idle level: SDA=%d SCL=%d", sda_level, scl_level);
    if (sda_level == 0 || scl_level == 0) {
        ESP_LOGW(TAG,
                 "I2C bus is held low; check pull-ups, device power, and GPIO%d/GPIO%d sharing",
                 config->sda_io_num, config->scl_io_num);
        return false;
    }
    return true;
}

static esp_err_t run_i2c_scan(void)
{
    i2c_master_bus_handle_t bus = NULL;
    i2c_master_bus_config_t *config = NULL;
    bool bus_referenced = false;

    esp_err_t ret = esp_board_periph_ref_handle("i2c_master", (void **)&bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "reference i2c_master failed: %s", esp_err_to_name(ret));
        return ret;
    }
    bus_referenced = true;

    ret = esp_board_manager_get_periph_config("i2c_master", (void **)&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get i2c_master config failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    if (!bus || !config) {
        ESP_LOGE(TAG, "Board Manager returned incomplete I2C data");
        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    esp_err_t touch_ret = esp_board_manager_init_device_by_name("lcd_touch");
    if (touch_ret == ESP_OK) {
        touch_ret = esp_board_manager_deinit_device_by_name("lcd_touch");
        if (touch_ret != ESP_OK) {
            ESP_LOGE(TAG, "touch release failed: %s", esp_err_to_name(touch_ret));
            ret = touch_ret;
            goto cleanup;
        }
    } else {
        ESP_LOGW(TAG, "touch wake-up failed: %s", esp_err_to_name(touch_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "I2C scan started: SDA=%d SCL=%d", config->sda_io_num, config->scl_io_num);

    /* The scan task intentionally owns this bus reference for its lifetime. */
    while (true) {
        if (!i2c_bus_is_idle(config)) {
            ESP_LOGW(TAG, "skip this scan while the bus is not idle");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        unsigned found = 0;
        unsigned timeout = 0;
        unsigned other_error = 0;
        esp_log_level_t old_level = esp_log_level_get(I2C_MASTER_TAG);
        esp_log_level_set(I2C_MASTER_TAG, ESP_LOG_NONE);
        for (uint8_t address = 0x03; address <= 0x77; address++) {
            esp_err_t probe_ret = i2c_master_probe(bus, address, SCAN_TIMEOUT_MS);
            if (probe_ret == ESP_OK) {
                ESP_LOGI(TAG, "found 0x%02x %s",
                         (unsigned)address, known_device_name(address));
                found++;
            } else if (probe_ret == ESP_ERR_TIMEOUT) {
                timeout++;
            } else if (probe_ret != ESP_ERR_NOT_FOUND) {
                other_error++;
                ESP_LOGW(TAG, "probe 0x%02x failed: %s",
                         (unsigned)address, esp_err_to_name(probe_ret));
            }
        }
        esp_log_level_set(I2C_MASTER_TAG, old_level);

        if (timeout) {
            ESP_LOGW(TAG, "scan done: %u device(s), %u timeout(s), %u other error(s)",
                     found, timeout, other_error);
            ESP_LOGW(TAG, "timeouts usually indicate a busy bus or weak pull-ups");
        } else {
            ESP_LOGI(TAG, "scan done: %u device(s), %u other error(s)", found, other_error);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }

cleanup:
    if (bus_referenced) {
        esp_err_t cleanup_ret = esp_board_periph_unref_handle("i2c_master");
        if (cleanup_ret != ESP_OK) {
            ESP_LOGE(TAG, "unreference i2c_master failed: %s",
                     esp_err_to_name(cleanup_ret));
            if (ret == ESP_OK) {
                ret = cleanup_ret;
            }
        }
    }
    return ret;
}

void app_main(void)
{
    esp_err_t ret = run_i2c_scan();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C scan stopped: %s", esp_err_to_name(ret));
    }
}
