#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_board_manager_includes.h"
#include "esp_cache.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_FRAME_BUFFER_COUNT 2
#define LCD_FIRST_FRAME_SETTLE_MS 50

static const char *TAG = "rgb_lcd";

static void sync_frame(uint16_t *frame, uint16_t width, uint16_t height)
{
    const size_t frame_size = (size_t)width * height * sizeof(*frame);

    ESP_ERROR_CHECK(esp_cache_msync(frame, frame_size,
                                    ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                                    ESP_CACHE_MSYNC_FLAG_UNALIGNED));
}

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3));
}

static void draw_pattern(uint16_t *frame, uint16_t width, uint16_t height, unsigned pattern)
{
    const uint16_t solid_colors[] = {0xffff, 0xf800, 0x07e0, 0x001f};
    const uint16_t bar_colors[] = {0xffff, 0xf800, 0x07e0, 0x001f, 0xffe0, 0xf81f, 0x07ff, 0x0000};
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint16_t color;
            if (pattern < 4) {
                color = solid_colors[pattern];
            } else if (pattern == 4) {
                color = bar_colors[(x * 8U) / width];
            } else if (pattern == 5) {
                color = (((x / 40) + (y / 40)) & 1) ? 0xffff : 0x0000;
            } else if (pattern == 6) {
                color = rgb565((x * 255U) / (width - 1),
                               (y * 255U) / (height - 1),
                               ((x + y) * 255U) / (width + height - 2));
            } else {
                bool border = x < 6 || x >= width - 6 || y < 6 || y >= height - 6;
                bool cross = (x >= width / 2 - 3 && x < width / 2 + 3) ||
                             (y >= height / 2 - 3 && y < height / 2 + 3);
                color = border || cross ? 0x0000 : 0xffff;
            }
            frame[y * width + x] = color;
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("display_lcd"));
    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *config = NULL;
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("display_lcd", (void **)&lcd));
    ESP_ERROR_CHECK(esp_board_manager_get_device_config("display_lcd", (void **)&config));
    uint16_t *frames[LCD_FRAME_BUFFER_COUNT] = {NULL, NULL};
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(lcd->panel_handle,
                                                       LCD_FRAME_BUFFER_COUNT,
                                                       (void **)&frames[0],
                                                       (void **)&frames[1]));
    ESP_ERROR_CHECK(frames[0] && frames[1] ? ESP_OK : ESP_ERR_INVALID_STATE);

    ESP_LOGI(TAG, "%ux%u RGB LCD pattern cycle started", config->lcd_width, config->lcd_height);
    unsigned pattern = 0;
    unsigned frame_index = 1;
    bool backlight_enabled = false;
    while (true) {
        uint16_t *frame = frames[frame_index];
        draw_pattern(frame, config->lcd_width, config->lcd_height, pattern);
        sync_frame(frame, config->lcd_width, config->lcd_height);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd->panel_handle,
                                                  0, 0,
                                                  config->lcd_width,
                                                  config->lcd_height,
                                                  frame));

        if (!backlight_enabled) {
            vTaskDelay(pdMS_TO_TICKS(LCD_FIRST_FRAME_SETTLE_MS));
            ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_brightness"));
            backlight_enabled = true;
        }

        pattern = (pattern + 1) % 8;
        vTaskDelay(pdMS_TO_TICKS(1000));
        frame_index ^= 1U;
    }
}
