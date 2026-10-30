#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "s31_board_support.h"

static const char *TAG = "rtc_pcf85063";

void app_main(void)
{
    s31_rtc_handle_t *rtc = NULL;
    ESP_ERROR_CHECK(s31_rtc_acquire(&rtc));
    ESP_ERROR_CHECK(s31_rtc_start_1hz_interrupt(rtc));

    s31_rtc_time_t now = {0};
    ESP_ERROR_CHECK(s31_rtc_get_time(rtc, &now));
    if (now.voltage_low || now.year < 2024 || now.month == 0 || now.day == 0) {
        const s31_rtc_time_t initial = {
            .year = 2026, .month = 1, .day = 1, .weekday = 4,
            .hour = 12, .minute = 0, .second = 0,
        };
        ESP_LOGW(TAG, "invalid RTC time; setting 2026-01-01 12:00:00");
        ESP_ERROR_CHECK(s31_rtc_set_time(rtc, &initial));
    }

    uint32_t last_interrupt = 0;
    while (true) {
        ESP_ERROR_CHECK(s31_rtc_get_time(rtc, &now));
        ESP_LOGI(TAG, "%04u-%02u-%02u %02u:%02u:%02u weekday=%u VL=%d",
                 now.year, now.month, now.day, now.hour, now.minute, now.second,
                 now.weekday, now.voltage_low);
        s31_rtc_interrupt_status_t status = {0};
        ESP_ERROR_CHECK(s31_rtc_get_interrupt_status(rtc, &status));
        if (status.count != last_interrupt) {
            ESP_LOGI(TAG, "INT count=%lu level=%d control2=0x%02x",
                     (unsigned long)status.count, status.gpio_level, status.control2);
            ESP_ERROR_CHECK(s31_rtc_clear_interrupt(rtc));
            last_interrupt = status.count;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
