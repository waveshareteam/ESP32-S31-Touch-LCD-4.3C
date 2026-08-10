#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_board_manager_includes.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MAX_POINTS        5
#define MARK_SIZE         11
#define CLEAR_BLOCK_W     96
#define CLEAR_BLOCK_H     64
#define CLEAR_BLOCK_MARGIN 12
#define COLOR_BACKGROUND  0xdefb
#define COLOR_CLEAR_BORDER 0x001f
#define COLOR_CLEAR_BLOCK 0x07ff

static const char *TAG = "touch";
static const uint16_t s_colors[MAX_POINTS] = {0xf800, 0x07e0, 0x001f, 0xfd20, 0xf81f};

static esp_err_t fill_screen(esp_lcd_panel_handle_t panel, uint16_t width,
                             uint16_t height, uint16_t *line)
{
    for (uint16_t x = 0; x < width; x++) {
        line[x] = COLOR_BACKGROUND;
    }
    for (uint16_t y = 0; y < height; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, y, width, y + 1, line);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_err_t draw_solid_rect(esp_lcd_panel_handle_t panel, uint16_t *line,
                                 int x0, int y0, int x1, int y1, uint16_t color)
{
    for (int x = x0; x < x1; x++) {
        line[x - x0] = color;
    }
    for (int y = y0; y < y1; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, x0, y, x1, y + 1, line);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_err_t draw_page(esp_lcd_panel_handle_t panel, uint16_t width,
                           uint16_t height, uint16_t *line)
{
    esp_err_t ret = fill_screen(panel, width, height, line);
    if (ret != ESP_OK) {
        return ret;
    }
    int x1 = width - CLEAR_BLOCK_MARGIN;
    int x0 = x1 - CLEAR_BLOCK_W;
    int y0 = CLEAR_BLOCK_MARGIN;
    int y1 = y0 + CLEAR_BLOCK_H;
    ret = draw_solid_rect(panel, line, x0, y0, x1, y1, COLOR_CLEAR_BORDER);
    if (ret == ESP_OK) {
        ret = draw_solid_rect(panel, line, x0 + 4, y0 + 4, x1 - 4, y1 - 4,
                              COLOR_CLEAR_BLOCK);
    }
    return ret;
}

static bool touch_in_clear_block(uint16_t x, uint16_t y, uint16_t width)
{
    int x1 = width - CLEAR_BLOCK_MARGIN;
    int x0 = x1 - CLEAR_BLOCK_W;
    return x >= x0 && x < x1 && y >= CLEAR_BLOCK_MARGIN &&
           y < CLEAR_BLOCK_MARGIN + CLEAR_BLOCK_H;
}

static esp_err_t draw_mark(esp_lcd_panel_handle_t panel, uint16_t width, uint16_t height,
                           uint16_t x, uint16_t y, uint16_t color, uint16_t *buffer)
{
    int x0 = (int)x - MARK_SIZE / 2;
    int y0 = (int)y - MARK_SIZE / 2;
    int x1 = x0 + MARK_SIZE;
    int y1 = y0 + MARK_SIZE;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width) x1 = width;
    if (y1 > height) y1 = height;
    for (int i = 0; i < (x1 - x0) * (y1 - y0); i++) {
        buffer[i] = color;
    }
    return esp_lcd_panel_draw_bitmap(panel, x0, y0, x1, y1, buffer);
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("display_lcd"));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_touch"));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name("lcd_brightness"));
    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *lcd_config = NULL;
    dev_lcd_touch_handles_t *touch = NULL;
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("display_lcd", (void **)&lcd));
    ESP_ERROR_CHECK(esp_board_manager_get_device_config("display_lcd", (void **)&lcd_config));
    ESP_ERROR_CHECK(esp_board_manager_get_device_handle("lcd_touch", (void **)&touch));

    uint16_t *line = heap_caps_malloc(lcd_config->lcd_width * sizeof(uint16_t),
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint16_t *mark = heap_caps_malloc(MARK_SIZE * MARK_SIZE * sizeof(uint16_t),
                                      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK(line && mark ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(draw_page(lcd->panel_handle, lcd_config->lcd_width,
                              lcd_config->lcd_height, line));

    uint32_t count = 0;
    while (true) {
        ESP_ERROR_CHECK(esp_lcd_touch_read_data(touch->touch_handle));
        uint16_t x[MAX_POINTS] = {0};
        uint16_t y[MAX_POINTS] = {0};
        uint16_t strength[MAX_POINTS] = {0};
        uint8_t points = 0;
        if (esp_lcd_touch_get_coordinates(touch->touch_handle, x, y, strength,
                                          &points, MAX_POINTS)) {
            bool clear_requested = false;
            for (uint8_t i = 0; i < points; i++) {
                if (touch_in_clear_block(x[i], y[i], lcd_config->lcd_width)) {
                    clear_requested = true;
                    break;
                }
            }
            if (clear_requested) {
                ESP_ERROR_CHECK(draw_page(lcd->panel_handle, lcd_config->lcd_width,
                                          lcd_config->lcd_height, line));
                ESP_LOGI(TAG, "clear screen");
            } else {
                for (uint8_t i = 0; i < points; i++) {
                    ESP_ERROR_CHECK(draw_mark(lcd->panel_handle, lcd_config->lcd_width,
                                              lcd_config->lcd_height, x[i], y[i],
                                              s_colors[i % MAX_POINTS], mark));
                    ESP_LOGI(TAG, "touch=%" PRIu32 " point=%u x=%u y=%u strength=%u",
                             count++, i, x[i], y[i], strength[i]);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
