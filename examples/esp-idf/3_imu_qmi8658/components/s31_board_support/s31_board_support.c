#include "s31_board_support.h"

#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gen_board_device_custom.h"
#include "soc/soc_caps.h"

#define I2C_TIMEOUT_MS 100

#define PCF85063_REG_CTRL1       0x00
#define PCF85063_REG_CTRL2       0x01
#define PCF85063_REG_SECONDS     0x04
#define PCF85063_REG_TIMER       0x10
#define PCF85063_REG_TIMER_MODE  0x11
#define PCF85063_TIME_LEN        7
#define PCF85063_CTRL2_AF        BIT(6)
#define PCF85063_CTRL2_TF        BIT(3)
#define PCF85063_TIMER_TE        BIT(4)
#define PCF85063_TIMER_TI_TP     BIT(3)
#define PCF85063_TIMER_TIE       BIT(2)
#define PCF85063_TIMER_TCF_1HZ   BIT(1)

#define QMI8658_REG_WHO_AM_I      0x00
#define QMI8658_REG_REVISION      0x01
#define QMI8658_REG_CTRL1         0x02
#define QMI8658_REG_CTRL2         0x03
#define QMI8658_REG_CTRL3         0x04
#define QMI8658_REG_CTRL7         0x08
#define QMI8658_REG_STATUS_INT    0x2d
#define QMI8658_REG_TEMP_L        0x33
#define QMI8658_WHO_AM_I          0x05
#define QMI8658_CTRL1_ADDR_AI     BIT(6)
#define QMI8658_CTRL7_ACC_ENABLE  BIT(0)
#define QMI8658_CTRL7_GYR_ENABLE  BIT(1)
#define QMI8658_STATUS0_READY     (BIT(0) | BIT(1))

#define BQ27220_CMD_TEMPERATURE  0x06
#define BQ27220_CMD_VOLTAGE      0x08
#define BQ27220_CMD_STATUS       0x0a
#define BQ27220_CMD_CURRENT      0x0c
#define BQ27220_CMD_REMAIN_CAP   0x10
#define BQ27220_CMD_FULL_CAP     0x12
#define BQ27220_CMD_AVG_CURRENT  0x14
#define BQ27220_CMD_TIME_EMPTY   0x16
#define BQ27220_CMD_TIME_FULL    0x18
#define BQ27220_CMD_MAX_LOAD     0x1e
#define BQ27220_CMD_AVG_POWER    0x24
#define BQ27220_CMD_CYCLE_COUNT  0x2a
#define BQ27220_CMD_SOC          0x2c
#define BQ27220_CMD_SOH          0x2e

static const char *TAG = "s31_board";
static bool s_ops_registered;

struct s31_rtc_handle {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    const char *bus_name;
    gpio_num_t int_gpio;
    volatile uint32_t interrupt_count;
    bool isr_added;
};

struct s31_imu_handle {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    const char *bus_name;
    gpio_num_t int_gpio;
};

struct s31_battery_handle {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    const char *bus_name;
};

struct s31_canfd_handle {
    twai_node_handle_t node;
    bool enabled;
};

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t)(((value >> 4) * 10) + (value & 0x0f));
}

static uint8_t bin_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static int16_t get_i16_le(const uint8_t *data)
{
    return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static esp_err_t add_i2c_device(i2c_master_bus_handle_t bus, uint8_t address,
                                uint32_t frequency, i2c_master_dev_handle_t *device)
{
    ESP_RETURN_ON_ERROR(i2c_master_probe(bus, address, I2C_TIMEOUT_MS), TAG,
                        "I2C device 0x%02x not found", address);
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = frequency,
    };
    return i2c_master_bus_add_device(bus, &config, device);
}

static esp_err_t managed_acquire(const char *name, void **handle)
{
    ESP_RETURN_ON_FALSE(name && handle, ESP_ERR_INVALID_ARG, TAG, "invalid managed device acquire");
    *handle = NULL;
    ESP_RETURN_ON_ERROR(s31_board_support_prepare(), TAG, "register board custom operations failed");
    ESP_RETURN_ON_ERROR(esp_board_manager_init_device_by_name(name), TAG,
                        "initialize %s failed", name);
    esp_err_t ret = esp_board_manager_get_device_handle(name, handle);
    if (ret != ESP_OK || *handle == NULL) {
        esp_board_manager_deinit_device_by_name(name);
        return ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
    }
    return ESP_OK;
}

static void IRAM_ATTR rtc_interrupt_handler(void *arg)
{
    s31_rtc_handle_t *handle = (s31_rtc_handle_t *)arg;
    handle->interrupt_count++;
}

static int rtc_init(void *raw_config, int config_size, void **device_handle)
{
    if (!raw_config || !device_handle ||
        config_size != sizeof(dev_custom_rtc_pcf85063_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    const dev_custom_rtc_pcf85063_config_t *config = raw_config;
    s31_rtc_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    handle->bus_name = config->peripheral_name;
    handle->int_gpio = (gpio_num_t)config->int_gpio;

    esp_err_t ret = esp_board_periph_ref_handle(handle->bus_name, (void **)&handle->bus);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = add_i2c_device(handle->bus, (uint8_t)config->address,
                         (uint32_t)config->frequency, &handle->device);
    if (ret != ESP_OK) {
        goto fail;
    }
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = BIT64(handle->int_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&gpio_cfg);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        goto fail;
    }
    ret = gpio_isr_handler_add(handle->int_gpio, rtc_interrupt_handler, handle);
    if (ret != ESP_OK) {
        goto fail;
    }
    handle->isr_added = true;
    const uint8_t ctrl[2] = {0, 0};
    uint8_t tx[3] = {PCF85063_REG_CTRL1, ctrl[0], ctrl[1]};
    ret = i2c_master_transmit(handle->device, tx, sizeof(tx), I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        goto fail;
    }
    *device_handle = handle;
    return ESP_OK;

fail:
    if (handle->isr_added) {
        gpio_isr_handler_remove(handle->int_gpio);
    }
    if (handle->device) {
        i2c_master_bus_rm_device(handle->device);
    }
    if (handle->bus) {
        esp_board_periph_unref_handle(handle->bus_name);
    }
    free(handle);
    return ret;
}

static int rtc_deinit(void *device_handle)
{
    s31_rtc_handle_t *handle = device_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->isr_added) {
        gpio_isr_handler_remove(handle->int_gpio);
    }
    gpio_reset_pin(handle->int_gpio);
    esp_err_t ret = i2c_master_bus_rm_device(handle->device);
    esp_err_t bus_ret = esp_board_periph_unref_handle(handle->bus_name);
    free(handle);
    return ret != ESP_OK ? ret : bus_ret;
}

static esp_err_t rtc_read(s31_rtc_handle_t *handle, uint8_t reg, uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "invalid RTC read");
    return i2c_master_transmit_receive(handle->device, &reg, 1, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t rtc_write(s31_rtc_handle_t *handle, uint8_t reg, const uint8_t *data, size_t len)
{
    ESP_RETURN_ON_FALSE(handle && data && len <= PCF85063_TIME_LEN,
                        ESP_ERR_INVALID_ARG, TAG, "invalid RTC write");
    uint8_t buffer[1 + PCF85063_TIME_LEN] = {reg};
    memcpy(buffer + 1, data, len);
    return i2c_master_transmit(handle->device, buffer, len + 1, I2C_TIMEOUT_MS);
}

static esp_err_t imu_read_reg(s31_imu_handle_t *handle, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(handle->device, &reg, 1, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t imu_write_reg(s31_imu_handle_t *handle, uint8_t reg, uint8_t value)
{
    uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(handle->device, buffer, sizeof(buffer), I2C_TIMEOUT_MS);
}

static int imu_init(void *raw_config, int config_size, void **device_handle)
{
    if (!raw_config || !device_handle ||
        config_size != sizeof(dev_custom_imu_qmi8658_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    const dev_custom_imu_qmi8658_config_t *config = raw_config;
    s31_imu_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    handle->bus_name = config->peripheral_name;
    handle->int_gpio = (gpio_num_t)config->int_gpio;
    esp_err_t ret = esp_board_periph_ref_handle(handle->bus_name, (void **)&handle->bus);
    if (ret != ESP_OK) {
        goto fail;
    }
    const uint8_t addresses[] = {(uint8_t)config->address, (uint8_t)config->fallback_address};
    for (size_t i = 0; i < sizeof(addresses); i++) {
        ret = add_i2c_device(handle->bus, addresses[i], (uint32_t)config->frequency, &handle->device);
        if (ret == ESP_OK) {
            break;
        }
    }
    if (!handle->device) {
        ret = ESP_ERR_NOT_FOUND;
        goto fail;
    }
    uint8_t who = 0;
    ret = imu_read_reg(handle, QMI8658_REG_WHO_AM_I, &who, 1);
    if (ret != ESP_OK || who != QMI8658_WHO_AM_I) {
        ret = ESP_ERR_NOT_FOUND;
        goto fail;
    }
    ESP_GOTO_ON_ERROR(imu_write_reg(handle, QMI8658_REG_CTRL1, QMI8658_CTRL1_ADDR_AI), fail, TAG, "CTRL1");
    ESP_GOTO_ON_ERROR(imu_write_reg(handle, QMI8658_REG_CTRL2, 0x23), fail, TAG, "CTRL2");
    ESP_GOTO_ON_ERROR(imu_write_reg(handle, QMI8658_REG_CTRL3, 0x53), fail, TAG, "CTRL3");
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = BIT64(handle->int_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&gpio_cfg), fail, TAG, "IMU interrupt GPIO");
    ESP_GOTO_ON_ERROR(imu_write_reg(handle, QMI8658_REG_CTRL7,
                                    QMI8658_CTRL7_ACC_ENABLE | QMI8658_CTRL7_GYR_ENABLE),
                      fail, TAG, "CTRL7");
    vTaskDelay(pdMS_TO_TICKS(50));
    *device_handle = handle;
    return ESP_OK;

fail:
    if (handle->device) {
        i2c_master_bus_rm_device(handle->device);
    }
    if (handle->bus) {
        esp_board_periph_unref_handle(handle->bus_name);
    }
    free(handle);
    return ret;
}

static int imu_deinit(void *device_handle)
{
    s31_imu_handle_t *handle = device_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    gpio_reset_pin(handle->int_gpio);
    esp_err_t ret = i2c_master_bus_rm_device(handle->device);
    esp_err_t bus_ret = esp_board_periph_unref_handle(handle->bus_name);
    free(handle);
    return ret != ESP_OK ? ret : bus_ret;
}

static int battery_init(void *raw_config, int config_size, void **device_handle)
{
    if (!raw_config || !device_handle ||
        config_size != sizeof(dev_custom_bat_bq27220_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    const dev_custom_bat_bq27220_config_t *config = raw_config;
    s31_battery_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    handle->bus_name = config->peripheral_name;
    esp_err_t ret = esp_board_periph_ref_handle(handle->bus_name, (void **)&handle->bus);
    if (ret == ESP_OK) {
        ret = add_i2c_device(handle->bus, (uint8_t)config->address,
                             (uint32_t)config->frequency, &handle->device);
    }
    if (ret != ESP_OK) {
        if (handle->bus) {
            esp_board_periph_unref_handle(handle->bus_name);
        }
        free(handle);
        return ret;
    }
    *device_handle = handle;
    return ESP_OK;
}

static int battery_deinit(void *device_handle)
{
    s31_battery_handle_t *handle = device_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = i2c_master_bus_rm_device(handle->device);
    esp_err_t bus_ret = esp_board_periph_unref_handle(handle->bus_name);
    free(handle);
    return ret != ESP_OK ? ret : bus_ret;
}

static esp_err_t battery_read_word(s31_battery_handle_t *handle, uint8_t command, uint16_t *value)
{
    ESP_RETURN_ON_FALSE(handle && value, ESP_ERR_INVALID_ARG, TAG, "invalid battery read");
    uint8_t raw[2] = {0};
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(handle->device, &command, 1,
                                                    raw, sizeof(raw), I2C_TIMEOUT_MS),
                        TAG, "battery command 0x%02x failed", command);
    *value = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    return ESP_OK;
}

static int canfd_init(void *raw_config, int config_size, void **device_handle)
{
    if (!raw_config || !device_handle ||
        config_size != sizeof(dev_custom_canfd_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
#if SOC_TWAI_FD_SUPPORTED
    const dev_custom_canfd_config_t *config = raw_config;
    s31_canfd_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }
    const twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = (gpio_num_t)config->tx_gpio,
            .rx = (gpio_num_t)config->rx_gpio,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .clk_src = TWAI_CLK_SRC_DEFAULT,
        .bit_timing.bitrate = (uint32_t)config->arbitration_bitrate,
        .data_timing.bitrate = (uint32_t)config->data_bitrate,
        .fail_retry_cnt = -1,
        .tx_queue_depth = 4,
    };
    esp_err_t ret = twai_new_node_onchip(&node_config, &handle->node);
    if (ret != ESP_OK) {
        free(handle);
        return ret;
    }
    *device_handle = handle;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static int canfd_deinit(void *device_handle)
{
    s31_canfd_handle_t *handle = device_handle;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
#if SOC_TWAI_FD_SUPPORTED
    esp_err_t ret = ESP_OK;
    if (handle->enabled) {
        ret = twai_node_disable(handle->node);
    }
    esp_err_t delete_ret = twai_node_delete(handle->node);
    free(handle);
    return ret != ESP_OK ? ret : delete_ret;
#else
    free(handle);
    return ESP_OK;
#endif
}

esp_err_t s31_board_support_prepare(void)
{
    if (s_ops_registered) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_board_device_set_ops(S31_DEVICE_RTC, rtc_init, rtc_deinit), TAG, "RTC ops");
    ESP_RETURN_ON_ERROR(esp_board_device_set_ops(S31_DEVICE_IMU, imu_init, imu_deinit), TAG, "IMU ops");
    ESP_RETURN_ON_ERROR(esp_board_device_set_ops(S31_DEVICE_BATTERY, battery_init, battery_deinit), TAG, "battery ops");
    ESP_RETURN_ON_ERROR(esp_board_device_set_ops(S31_DEVICE_CANFD, canfd_init, canfd_deinit), TAG, "CAN-FD ops");
    s_ops_registered = true;
    return ESP_OK;
}

esp_err_t s31_rtc_acquire(s31_rtc_handle_t **handle)
{
    return managed_acquire(S31_DEVICE_RTC, (void **)handle);
}

esp_err_t s31_rtc_release(void)
{
    return esp_board_manager_deinit_device_by_name(S31_DEVICE_RTC);
}

esp_err_t s31_rtc_get_time(s31_rtc_handle_t *handle, s31_rtc_time_t *time)
{
    ESP_RETURN_ON_FALSE(time, ESP_ERR_INVALID_ARG, TAG, "RTC time output is null");
    uint8_t raw[PCF85063_TIME_LEN] = {0};
    ESP_RETURN_ON_ERROR(rtc_read(handle, PCF85063_REG_SECONDS, raw, sizeof(raw)), TAG, "RTC time");
    time->voltage_low = (raw[0] & BIT(7)) != 0;
    time->second = bcd_to_bin(raw[0] & 0x7f);
    time->minute = bcd_to_bin(raw[1] & 0x7f);
    time->hour = bcd_to_bin(raw[2] & 0x3f);
    time->day = bcd_to_bin(raw[3] & 0x3f);
    time->weekday = bcd_to_bin(raw[4] & 0x07);
    time->month = bcd_to_bin(raw[5] & 0x1f);
    time->year = 2000 + bcd_to_bin(raw[6]);
    return ESP_OK;
}

esp_err_t s31_rtc_set_time(s31_rtc_handle_t *handle, const s31_rtc_time_t *time)
{
    ESP_RETURN_ON_FALSE(time, ESP_ERR_INVALID_ARG, TAG, "RTC time input is null");
    const uint8_t raw[PCF85063_TIME_LEN] = {
        bin_to_bcd(time->second), bin_to_bcd(time->minute), bin_to_bcd(time->hour),
        bin_to_bcd(time->day), bin_to_bcd(time->weekday), bin_to_bcd(time->month),
        bin_to_bcd((uint8_t)(time->year % 100)),
    };
    return rtc_write(handle, PCF85063_REG_SECONDS, raw, sizeof(raw));
}

esp_err_t s31_rtc_clear_interrupt(s31_rtc_handle_t *handle)
{
    uint8_t ctrl2 = 0;
    ESP_RETURN_ON_ERROR(rtc_read(handle, PCF85063_REG_CTRL2, &ctrl2, 1), TAG, "RTC control2");
    ctrl2 &= (uint8_t)~(PCF85063_CTRL2_AF | PCF85063_CTRL2_TF);
    return rtc_write(handle, PCF85063_REG_CTRL2, &ctrl2, 1);
}

esp_err_t s31_rtc_start_1hz_interrupt(s31_rtc_handle_t *handle)
{
    uint8_t value = 0;
    ESP_RETURN_ON_ERROR(rtc_write(handle, PCF85063_REG_TIMER_MODE, &value, 1), TAG, "disable timer");
    ESP_RETURN_ON_ERROR(s31_rtc_clear_interrupt(handle), TAG, "clear timer flags");
    value = 1;
    ESP_RETURN_ON_ERROR(rtc_write(handle, PCF85063_REG_TIMER, &value, 1), TAG, "timer value");
    value = PCF85063_TIMER_TE | PCF85063_TIMER_TI_TP | PCF85063_TIMER_TIE | PCF85063_TIMER_TCF_1HZ;
    return rtc_write(handle, PCF85063_REG_TIMER_MODE, &value, 1);
}

esp_err_t s31_rtc_get_interrupt_status(s31_rtc_handle_t *handle, s31_rtc_interrupt_status_t *status)
{
    ESP_RETURN_ON_FALSE(handle && status, ESP_ERR_INVALID_ARG, TAG, "invalid RTC status");
    ESP_RETURN_ON_ERROR(rtc_read(handle, PCF85063_REG_CTRL2, &status->control2, 1), TAG, "RTC control2");
    status->count = handle->interrupt_count;
    status->gpio_level = gpio_get_level(handle->int_gpio);
    return ESP_OK;
}

esp_err_t s31_imu_acquire(s31_imu_handle_t **handle)
{
    return managed_acquire(S31_DEVICE_IMU, (void **)handle);
}

esp_err_t s31_imu_release(void)
{
    return esp_board_manager_deinit_device_by_name(S31_DEVICE_IMU);
}

esp_err_t s31_imu_get_identity(s31_imu_handle_t *handle, uint8_t *who_am_i, uint8_t *revision)
{
    ESP_RETURN_ON_FALSE(handle && who_am_i, ESP_ERR_INVALID_ARG, TAG, "invalid IMU identity");
    ESP_RETURN_ON_ERROR(imu_read_reg(handle, QMI8658_REG_WHO_AM_I, who_am_i, 1), TAG, "WHO_AM_I");
    if (revision) {
        ESP_RETURN_ON_ERROR(imu_read_reg(handle, QMI8658_REG_REVISION, revision, 1), TAG, "revision");
    }
    return ESP_OK;
}

esp_err_t s31_imu_is_data_ready(s31_imu_handle_t *handle, bool *ready,
                                uint8_t *status_int, uint8_t *status0)
{
    ESP_RETURN_ON_FALSE(handle && ready, ESP_ERR_INVALID_ARG, TAG, "invalid IMU status");
    uint8_t status[2] = {0};
    ESP_RETURN_ON_ERROR(imu_read_reg(handle, QMI8658_REG_STATUS_INT, status, sizeof(status)),
                        TAG, "IMU status");
    *ready = (status[1] & QMI8658_STATUS0_READY) == QMI8658_STATUS0_READY;
    if (status_int) {
        *status_int = status[0];
    }
    if (status0) {
        *status0 = status[1];
    }
    return ESP_OK;
}

esp_err_t s31_imu_read(s31_imu_handle_t *handle, s31_imu_data_t *data)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, TAG, "invalid IMU data");
    uint8_t status[2] = {0};
    uint8_t raw[14] = {0};
    ESP_RETURN_ON_ERROR(imu_read_reg(handle, QMI8658_REG_STATUS_INT, status, sizeof(status)),
                        TAG, "IMU status");
    ESP_RETURN_ON_ERROR(imu_read_reg(handle, QMI8658_REG_TEMP_L, raw, sizeof(raw)),
                        TAG, "IMU sample");
    data->temperature_c = get_i16_le(raw) / 256.0f;
    data->accel_x_g = get_i16_le(raw + 2) * 8.0f / 32768.0f;
    data->accel_y_g = get_i16_le(raw + 4) * 8.0f / 32768.0f;
    data->accel_z_g = get_i16_le(raw + 6) * 8.0f / 32768.0f;
    data->gyro_x_dps = get_i16_le(raw + 8) * 512.0f / 32768.0f;
    data->gyro_y_dps = get_i16_le(raw + 10) * 512.0f / 32768.0f;
    data->gyro_z_dps = get_i16_le(raw + 12) * 512.0f / 32768.0f;
    data->status_int = status[0];
    data->status0 = status[1];
    data->int_gpio_level = gpio_get_level(handle->int_gpio);
    return ESP_OK;
}

esp_err_t s31_battery_acquire(s31_battery_handle_t **handle)
{
    return managed_acquire(S31_DEVICE_BATTERY, (void **)handle);
}

esp_err_t s31_battery_release(void)
{
    return esp_board_manager_deinit_device_by_name(S31_DEVICE_BATTERY);
}

esp_err_t s31_battery_get_info(s31_battery_handle_t *handle, s31_battery_info_t *info)
{
    ESP_RETURN_ON_FALSE(handle && info, ESP_ERR_INVALID_ARG, TAG, "invalid battery info");
    memset(info, 0, sizeof(*info));
    uint16_t temperature = 0;
    uint16_t value = 0;
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_TEMPERATURE, &temperature), TAG, "temperature");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_VOLTAGE, &info->voltage_mv), TAG, "voltage");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_STATUS, &info->status), TAG, "status");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_CURRENT, &value), TAG, "current");
    info->current_ma = (int16_t)value;
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_AVG_CURRENT, &value), TAG, "average current");
    info->average_current_ma = (int16_t)value;
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_REMAIN_CAP, &info->remaining_capacity_mah), TAG, "remaining capacity");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_FULL_CAP, &info->full_charge_capacity_mah), TAG, "full capacity");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_TIME_EMPTY, &info->time_to_empty_min), TAG, "time empty");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_TIME_FULL, &info->time_to_full_min), TAG, "time full");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_MAX_LOAD, &value), TAG, "max load");
    info->max_load_ma = (int16_t)value;
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_AVG_POWER, &value), TAG, "average power");
    info->average_power_mw = (int16_t)value;
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_CYCLE_COUNT, &info->cycle_count), TAG, "cycles");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_SOC, &info->state_of_charge_percent), TAG, "SOC");
    ESP_RETURN_ON_ERROR(battery_read_word(handle, BQ27220_CMD_SOH, &info->state_of_health_percent), TAG, "SOH");
    info->state_of_health_percent &= 0xff;
    info->temperature_c = temperature / 10.0f - 273.15f;
    return ESP_OK;
}

esp_err_t s31_canfd_acquire(s31_canfd_handle_t **handle)
{
    return managed_acquire(S31_DEVICE_CANFD, (void **)handle);
}

esp_err_t s31_canfd_release(void)
{
    return esp_board_manager_deinit_device_by_name(S31_DEVICE_CANFD);
}

twai_node_handle_t s31_canfd_node(s31_canfd_handle_t *handle)
{
    return handle ? handle->node : NULL;
}

esp_err_t s31_canfd_enable(s31_canfd_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "CAN-FD handle is null");
    if (handle->enabled) {
        return ESP_OK;
    }
    esp_err_t ret = twai_node_enable(handle->node);
    if (ret == ESP_OK) {
        handle->enabled = true;
    }
    return ret;
}
