#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_twai.h"

#ifdef __cplusplus
extern "C" {
#endif

#define S31_DEVICE_RTC       "rtc_pcf85063"
#define S31_DEVICE_IMU       "imu_qmi8658"
#define S31_DEVICE_BATTERY   "bat_bq27220"
#define S31_DEVICE_CANFD     "canfd"

typedef struct s31_rtc_handle s31_rtc_handle_t;
typedef struct s31_imu_handle s31_imu_handle_t;
typedef struct s31_battery_handle s31_battery_handle_t;
typedef struct s31_canfd_handle s31_canfd_handle_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool voltage_low;
} s31_rtc_time_t;

typedef struct {
    uint32_t count;
    int gpio_level;
    uint8_t control2;
} s31_rtc_interrupt_status_t;

typedef struct {
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;
    float temperature_c;
    uint8_t status_int;
    uint8_t status0;
    int int_gpio_level;
} s31_imu_data_t;

typedef struct {
    uint16_t voltage_mv;
    int16_t current_ma;
    int16_t average_current_ma;
    int16_t max_load_ma;
    int16_t average_power_mw;
    uint16_t remaining_capacity_mah;
    uint16_t full_charge_capacity_mah;
    uint16_t state_of_charge_percent;
    uint16_t state_of_health_percent;
    float temperature_c;
    uint16_t time_to_empty_min;
    uint16_t time_to_full_min;
    uint16_t cycle_count;
    uint16_t status;
} s31_battery_info_t;

esp_err_t s31_board_support_prepare(void);

esp_err_t s31_rtc_acquire(s31_rtc_handle_t **handle);
esp_err_t s31_rtc_release(void);
esp_err_t s31_rtc_get_time(s31_rtc_handle_t *handle, s31_rtc_time_t *time);
esp_err_t s31_rtc_set_time(s31_rtc_handle_t *handle, const s31_rtc_time_t *time);
esp_err_t s31_rtc_start_1hz_interrupt(s31_rtc_handle_t *handle);
esp_err_t s31_rtc_clear_interrupt(s31_rtc_handle_t *handle);
esp_err_t s31_rtc_get_interrupt_status(s31_rtc_handle_t *handle, s31_rtc_interrupt_status_t *status);

esp_err_t s31_imu_acquire(s31_imu_handle_t **handle);
esp_err_t s31_imu_release(void);
esp_err_t s31_imu_get_identity(s31_imu_handle_t *handle, uint8_t *who_am_i, uint8_t *revision);
esp_err_t s31_imu_is_data_ready(s31_imu_handle_t *handle, bool *ready, uint8_t *status_int, uint8_t *status0);
esp_err_t s31_imu_read(s31_imu_handle_t *handle, s31_imu_data_t *data);

esp_err_t s31_battery_acquire(s31_battery_handle_t **handle);
esp_err_t s31_battery_release(void);
esp_err_t s31_battery_get_info(s31_battery_handle_t *handle, s31_battery_info_t *info);

esp_err_t s31_canfd_acquire(s31_canfd_handle_t **handle);
esp_err_t s31_canfd_release(void);
twai_node_handle_t s31_canfd_node(s31_canfd_handle_t *handle);
esp_err_t s31_canfd_enable(s31_canfd_handle_t *handle);

#ifdef __cplusplus
}
#endif
