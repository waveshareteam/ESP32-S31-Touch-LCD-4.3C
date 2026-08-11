#include "app_media.h"
#include "app_ui.h"
#include "esp_board_manager_includes.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define VIDEO_PERIOD_MS (1000 / CONFIG_S31_LVGL_DEMO_VIDEO_FPS)

static const char *TAG = "lvgl_port";

static void video_task(void *arg)
{
    (void)arg;
    uint32_t frame = 0;
    const TickType_t period = pdMS_TO_TICKS(VIDEO_PERIOD_MS > 0 ? VIDEO_PERIOD_MS : 1);
    while (true) {
        app_media_render_video_frame(frame++);
        app_ui_notify_video_frame();
        vTaskDelay(app_ui_turbo_enabled() ? period : pdMS_TO_TICKS(66));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LVGL %d.%d.%d porting demo",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    ESP_ERROR_CHECK(app_media_init());
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("display_lcd"));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_touch"));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_brightness"));

    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *lcd_config = NULL;
    dev_lcd_touch_handles_t *touch = NULL;
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("display_lcd", (void **)&lcd));
    ESP_ERROR_CHECK(esp_board_manager_get_device_config("display_lcd", (void **)&lcd_config));
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("lcd_touch", (void **)&touch));

    esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter.task_priority = 4;
    adapter.task_stack_size = 8192;
    adapter.tick_period_ms = 5;
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter));

    esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(lcd->panel_handle, lcd->io_handle,
                                                   lcd_config->lcd_width, lcd_config->lcd_height,
                                                   ESP_LV_ADAPTER_ROTATE_0);
    display_config.profile.buffer_height = 80;
    display_config.profile.use_psram = true;
    display_config.profile.require_double_buffer = true;
    display_config.profile.enable_ppa_accel = CONFIG_S31_LVGL_DEMO_ENABLE_PPA;
    display_config.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_DIRECT;
    lv_display_t *display = esp_lv_adapter_register_display(&display_config);
    ESP_ERROR_CHECK(display ? ESP_OK : ESP_FAIL);

    const esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch->touch_handle);
    ESP_ERROR_CHECK(esp_lv_adapter_register_touch(&touch_config) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_lv_adapter_start());

    ESP_ERROR_CHECK(esp_lv_adapter_lock(-1));
    app_ui_create();
    app_ui_start();
    esp_lv_adapter_unlock();
    ESP_ERROR_CHECK(xTaskCreate(video_task, "lv_video", 6144, NULL, 8, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "LVGL media demo running through Board Manager");
}
