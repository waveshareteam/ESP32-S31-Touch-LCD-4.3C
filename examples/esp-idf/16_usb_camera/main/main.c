/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_board_manager_includes.h"
#include "s31_board_support.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "usb/usb_host.h"
#include "usb/uvc_host.h"

#define CAMERA_H_RES                    640
#define CAMERA_V_RES                    480
#define CAMERA_FPS                      30
#define CAMERA_FORMAT                   UVC_VS_FORMAT_MJPEG

#define PREVIEW_H_RES                   640
#define PREVIEW_V_RES                   480
#define PREVIEW_EYE_LEFT                0
#define PREVIEW_EYE_RIGHT               1
#define PREVIEW_EYE                     PREVIEW_EYE_RIGHT

#define FALLBACK_H_RES                  640
#define FALLBACK_V_RES                  480

#define USB_TASK_PRIORITY               15
#define FRAME_TASK_PRIORITY             5
#define FRAME_TASK_STACK_SIZE           (8 * 1024)
#define FRAME_QUEUE_LEN                 1

#define UVC_FRAME_BUFFER_COUNT          4
#define UVC_URB_COUNT                   5
#define UVC_URB_SIZE                    (16 * 1024)

/* Decode one frame out of N. Use 1 for maximum preview rate if the board can keep up. */
#define DECODE_EVERY_NTH_FRAME          3

#define NO_CAMERA_MESSAGE               "Camera not detected"

#if CONFIG_FREERTOS_UNICORE
#define USB_TASK_CORE                   0
#define FRAME_TASK_CORE                 0
#else
#define USB_TASK_CORE                   0
#define FRAME_TASK_CORE                 1
#endif

#define DWFRAMEINTERVAL_TO_FPS(interval) (((interval) != 0) ? 10000000.0f / (float)(interval) : 0.0f)

static const char *TAG = "usb_camera";

static QueueHandle_t s_frame_queue;
static esp_lcd_panel_handle_t s_lcd_panel;
static uint16_t s_lcd_width;
static uint16_t s_lcd_height;
static uint8_t *s_rgb565_buf;
static jpeg_dec_handle_t s_jpeg_dec;
static volatile bool s_streaming;
static uint32_t s_rx_frame_count;

static bool camera_frame_cb(const uvc_host_frame_t *frame, void *user_ctx);
static void camera_stream_cb(const uvc_host_stream_event_data_t *event, void *user_ctx);
static void camera_driver_cb(const uvc_host_driver_event_data_t *event, void *user_ctx);

static const uvc_host_stream_format_t s_preferred_formats[] = {
    {
        .h_res = CAMERA_H_RES,
        .v_res = CAMERA_V_RES,
        .fps = CAMERA_FPS,
        .format = CAMERA_FORMAT,
    },
    {
        .h_res = CAMERA_H_RES,
        .v_res = CAMERA_V_RES,
        .fps = 15,
        .format = CAMERA_FORMAT,
    },
    {
        .h_res = CAMERA_H_RES,
        .v_res = CAMERA_V_RES,
        .fps = 0,
        .format = CAMERA_FORMAT,
    },
    {
        .h_res = FALLBACK_H_RES,
        .v_res = FALLBACK_V_RES,
        .fps = CAMERA_FPS,
        .format = CAMERA_FORMAT,
    },
    {
        .h_res = FALLBACK_H_RES,
        .v_res = FALLBACK_V_RES,
        .fps = 15,
        .format = CAMERA_FORMAT,
    },
    {
        .h_res = FALLBACK_H_RES,
        .v_res = FALLBACK_V_RES,
        .fps = 0,
        .format = CAMERA_FORMAT,
    },
};

static const uvc_host_stream_config_t s_stream_config = {
    .event_cb = camera_stream_cb,
    .frame_cb = camera_frame_cb,
    .user_ctx = &s_frame_queue,
    .usb = {
        .vid = UVC_HOST_ANY_VID,
        .pid = UVC_HOST_ANY_PID,
        .uvc_stream_index = 0,
    },
    .vs_format = {
        .h_res = CAMERA_H_RES,
        .v_res = CAMERA_V_RES,
        .fps = CAMERA_FPS,
        .format = CAMERA_FORMAT,
    },
    .advanced = {
        .number_of_frame_buffers = UVC_FRAME_BUFFER_COUNT,
        .frame_size = 0,
        .frame_heap_caps = MALLOC_CAP_SPIRAM,
        .number_of_urbs = UVC_URB_COUNT,
        .urb_size = UVC_URB_SIZE,
    },
};

static const char *format_to_name(enum uvc_host_stream_format format)
{
    switch (format) {
    case UVC_VS_FORMAT_MJPEG:
        return "MJPEG";
    case UVC_VS_FORMAT_YUY2:
        return "YUY2";
    case UVC_VS_FORMAT_H264:
        return "H264";
    case UVC_VS_FORMAT_H265:
        return "H265";
    case UVC_VS_FORMAT_NV12:
        return "NV12";
    default:
        return "UNKNOWN";
    }
}

static void log_stream_format(const char *prefix, const uvc_host_stream_format_t *format)
{
    if (format->fps > 0.0f) {
        ESP_LOGI(TAG, "%s %s %ux%u@%.1ffps", prefix, format_to_name(format->format),
                 format->h_res, format->v_res, format->fps);
    } else {
        ESP_LOGI(TAG, "%s %s %ux%u@default fps", prefix, format_to_name(format->format),
                 format->h_res, format->v_res);
    }
}

static esp_err_t default_font_render_text(const char *text, uint8_t **bitmap_out,
                                          int *width_out, int *height_out)
{
    if (!text || !bitmap_out || !width_out || !height_out) {
        return ESP_ERR_INVALID_ARG;
    }

    const lv_font_t *font = &lv_font_montserrat_14;

    int text_width = 0;
    const size_t character_count = strlen(text);
    for (size_t i = 0; i < character_count; i++) {
        const uint32_t letter = (uint8_t)text[i];
        const uint32_t letter_next = (i + 1 < character_count) ? (uint8_t)text[i + 1] : 0;
        text_width += lv_font_get_glyph_width(font, letter, letter_next);
    }

    const int text_height = lv_font_get_line_height(font);
    if (text_width <= 0 || text_height <= 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *text_bitmap = heap_caps_calloc((size_t)text_width * text_height, sizeof(uint8_t),
                                            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!text_bitmap) {
        return ESP_ERR_NO_MEM;
    }

    int pen_x = 0;
    for (size_t i = 0; i < character_count; i++) {
        const uint32_t letter = (uint8_t)text[i];
        const uint32_t letter_next = (i + 1 < character_count) ? (uint8_t)text[i + 1] : 0;
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(font, &glyph, letter, letter_next)) {
            free(text_bitmap);
            return ESP_ERR_NOT_FOUND;
        }

        if (glyph.box_w > 0 && glyph.box_h > 0) {
            lv_draw_buf_t *glyph_buf =
                lv_draw_buf_create(glyph.box_w, glyph.box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
            if (!glyph_buf) {
                lv_font_glyph_release_draw_data(&glyph);
                free(text_bitmap);
                return ESP_ERR_NO_MEM;
            }

            if (!lv_font_get_glyph_bitmap(&glyph, glyph_buf)) {
                lv_font_glyph_release_draw_data(&glyph);
                lv_draw_buf_destroy(glyph_buf);
                free(text_bitmap);
                return ESP_FAIL;
            }
            const uint8_t *glyph_bitmap = glyph_buf->data;

            const int glyph_x = pen_x + glyph.ofs_x;
            const int glyph_y = text_height - font->base_line - glyph.box_h - glyph.ofs_y;
            for (int y = 0; y < glyph.box_h; y++) {
                const int target_y = glyph_y + y;
                if (target_y < 0 || target_y >= text_height) {
                    continue;
                }

                for (int x = 0; x < glyph.box_w; x++) {
                    const int target_x = glyph_x + x;
                    if (target_x < 0 || target_x >= text_width) {
                        continue;
                    }

                    const uint8_t alpha = glyph_bitmap[y * glyph_buf->header.stride + x];
                    uint8_t *target = &text_bitmap[target_y * text_width + target_x];
                    if (alpha > *target) {
                        *target = alpha;
                    }
                }
            }

            lv_font_glyph_release_draw_data(&glyph);
            lv_draw_buf_destroy(glyph_buf);
        } else {
            lv_font_glyph_release_draw_data(&glyph);
        }

        pen_x += glyph.adv_w;
    }

    *bitmap_out = text_bitmap;
    *width_out = text_width;
    *height_out = text_height;
    return ESP_OK;
}

static esp_err_t lcd_draw_no_camera_message(void)
{
    if (!s_lcd_panel || s_lcd_width == 0 || s_lcd_height == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t *text_bitmap = NULL;
    int text_width = 0;
    int text_height = 0;
    esp_err_t ret = default_font_render_text(NO_CAMERA_MESSAGE, &text_bitmap,
                                             &text_width, &text_height);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t *line = heap_caps_malloc(s_lcd_width * sizeof(uint16_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!line) {
        free(text_bitmap);
        return ESP_ERR_NO_MEM;
    }

    const int x0 = (s_lcd_width > text_width) ? (s_lcd_width - text_width) / 2 : 0;
    const int y0 = (s_lcd_height > text_height) ? (s_lcd_height - text_height) / 2 : 0;

    for (int y = 0; y < s_lcd_height; y++) {
        memset(line, 0, s_lcd_width * sizeof(uint16_t));

        if (y >= y0 && y < y0 + text_height) {
            const uint8_t *text_line = &text_bitmap[(y - y0) * text_width];
            const int draw_width = (x0 + text_width <= s_lcd_width) ?
                                   text_width : s_lcd_width - x0;
            for (int x = 0; x < draw_width; x++) {
                const uint8_t alpha = text_line[x];
                line[x0 + x] = ((uint16_t)(alpha & 0xf8) << 8) |
                               ((uint16_t)(alpha & 0xfc) << 3) |
                               (alpha >> 3);
            }
        }

        ret = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, y, s_lcd_width, y + 1, line);
        if (ret != ESP_OK) {
            break;
        }
    }

    free(line);
    free(text_bitmap);
    return ret;
}

static void lcd_show_no_camera_message(void)
{
    esp_err_t ret = lcd_draw_no_camera_message();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to draw no-camera message: %s", esp_err_to_name(ret));
    }
}

static esp_err_t lcd_init(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name("display_lcd");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_board_manager_init_device_by_name("lcd_brightness");
    if (ret != ESP_OK) {
        esp_board_manager_deinit_device_by_name("display_lcd");
        return ret;
    }

    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *config = NULL;
    ret = esp_board_manager_get_device_handle("display_lcd", (void **)&lcd);
    if (ret == ESP_OK) {
        ret = esp_board_manager_get_device_config("display_lcd", (void **)&config);
    }
    if (ret != ESP_OK || !lcd || !lcd->panel_handle || !config) {
        ret = ret == ESP_OK ? ESP_ERR_INVALID_STATE : ret;
        goto cleanup;
    }

    s_lcd_panel = lcd->panel_handle;
    s_lcd_width = config->lcd_width;
    s_lcd_height = config->lcd_height;
    uint16_t *line = heap_caps_calloc(s_lcd_width, sizeof(uint16_t),
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!line) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    for (uint16_t y = 0; y < s_lcd_height; y++) {
        ret = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, y, s_lcd_width, y + 1, line);
        if (ret != ESP_OK) {
            break;
        }
    }
    free(line);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

cleanup:
    s_lcd_panel = NULL;
    s_lcd_width = 0;
    s_lcd_height = 0;
    esp_board_manager_deinit_device_by_name("lcd_brightness");
    esp_board_manager_deinit_device_by_name("display_lcd");
    return ret;
}

static void lcd_deinit(void)
{
    if (s_lcd_panel) {
        esp_board_manager_deinit_device_by_name("lcd_brightness");
        esp_board_manager_deinit_device_by_name("display_lcd");
        s_lcd_panel = NULL;
        s_lcd_width = 0;
        s_lcd_height = 0;
    }
}

static esp_err_t jpeg_decoder_init(void)
{
    if (s_jpeg_dec) {
        return ESP_OK;
    }

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;

    jpeg_error_t ret = jpeg_dec_open(&config, &s_jpeg_dec);
    ESP_RETURN_ON_FALSE(ret == JPEG_ERR_OK, ESP_FAIL, TAG, "jpeg_dec_open failed: %d", ret);
    return ESP_OK;
}

static esp_err_t decode_mjpeg(const uvc_host_frame_t *frame, uint32_t *out_width, uint32_t *out_height)
{
    ESP_RETURN_ON_FALSE(frame->vs_format.format == CAMERA_FORMAT, ESP_ERR_NOT_SUPPORTED,
                        TAG, "Unsupported frame format: %s", format_to_name(frame->vs_format.format));

    ESP_RETURN_ON_ERROR(jpeg_decoder_init(), TAG, "JPEG decoder init failed");

    jpeg_dec_io_t io = {
        .inbuf = frame->data,
        .inbuf_len = (int)frame->data_len,
        .outbuf = s_rgb565_buf,
    };
    jpeg_dec_header_info_t info = {0};

    jpeg_error_t ret = jpeg_dec_parse_header(s_jpeg_dec, &io, &info);
    ESP_RETURN_ON_FALSE(ret == JPEG_ERR_OK, ESP_FAIL, TAG, "JPEG header parse failed: %d", ret);
    ESP_RETURN_ON_FALSE(info.width <= CAMERA_H_RES && info.height <= CAMERA_V_RES,
                        ESP_ERR_INVALID_SIZE, TAG, "Unexpected JPEG size: %ux%u", info.width, info.height);

    ret = jpeg_dec_process(s_jpeg_dec, &io);
    ESP_RETURN_ON_FALSE(ret == JPEG_ERR_OK, ESP_FAIL, TAG, "JPEG decode failed: %d", ret);

    *out_width = info.width;
    *out_height = info.height;
    return ESP_OK;
}

static esp_err_t draw_camera_frame(const uvc_host_frame_t *frame)
{
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    ESP_RETURN_ON_ERROR(decode_mjpeg(frame, &frame_width, &frame_height), TAG, "MJPEG decode failed");

    const int draw_width = (frame_width > PREVIEW_H_RES) ? PREVIEW_H_RES : (int)frame_width;
    const int draw_height = (frame_height > PREVIEW_V_RES) ? PREVIEW_V_RES : (int)frame_height;
    const int src_x = (PREVIEW_EYE == PREVIEW_EYE_RIGHT && frame_width > PREVIEW_H_RES) ?
                      ((int)frame_width - PREVIEW_H_RES) : 0;
    const int x0 = (s_lcd_width - draw_width) / 2;
    const int y0 = (s_lcd_height - draw_height) / 2;
    const uint16_t *src = (const uint16_t *)s_rgb565_buf;

    /* Draw one eye from a side-by-side frame, or the full frame for 640x480 fallback. */
    for (int y = 0; y < draw_height; y++) {
        const uint16_t *line = src + ((y * frame_width) + src_x);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_lcd_panel,
                            x0, y0 + y, x0 + draw_width, y0 + y + 1, line),
                            TAG, "LCD draw line failed");
    }

    return ESP_OK;
}

static bool camera_frame_cb(const uvc_host_frame_t *frame, void *user_ctx)
{
    QueueHandle_t queue = *((QueueHandle_t *)user_ctx);

    if ((s_rx_frame_count++ % DECODE_EVERY_NTH_FRAME) != 0) {
        return true;
    }

    if (xQueueSendToBack(queue, &frame, 0) != pdPASS) {
        return true;
    }

    return false;
}

static void camera_stream_cb(const uvc_host_stream_event_data_t *event, void *user_ctx)
{
    (void)user_ctx;

    switch (event->type) {
    case UVC_HOST_TRANSFER_ERROR:
        ESP_LOGW(TAG, "USB transfer error: %s", esp_err_to_name(event->transfer_error.error));
        break;
    case UVC_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "UVC camera disconnected");
        s_streaming = false;
        break;
    case UVC_HOST_FRAME_BUFFER_OVERFLOW:
        ESP_LOGW(TAG, "Frame buffer overflow");
        break;
    case UVC_HOST_FRAME_BUFFER_UNDERFLOW:
        ESP_LOGW(TAG, "Frame buffer underflow");
        break;
    default:
        ESP_LOGD(TAG, "UVC stream event: %d", event->type);
        break;
    }
}

static void camera_driver_cb(const uvc_host_driver_event_data_t *event, void *user_ctx)
{
    (void)user_ctx;

    if (event->type != UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED) {
        return;
    }

    ESP_LOGI(TAG, "UVC camera connected, addr=%u, stream_index=%u",
             event->device_connected.dev_addr, event->device_connected.uvc_stream_index);

    size_t format_count = event->device_connected.frame_info_num;
    if (format_count == 0) {
        return;
    }

    uvc_host_frame_info_t *formats = calloc(format_count, sizeof(uvc_host_frame_info_t));
    if (!formats) {
        ESP_LOGW(TAG, "No memory to list camera formats");
        return;
    }

    esp_err_t ret = uvc_host_get_frame_list(event->device_connected.dev_addr,
                                            event->device_connected.uvc_stream_index,
                                            (uvc_host_frame_info_t (*)[])formats,
                                            &format_count);
    if (ret == ESP_OK) {
        for (size_t i = 0; i < format_count; i++) {
            ESP_LOGI(TAG, "Camera format[%u]: %s %ux%u default %.1ffps",
                     (unsigned)i, format_to_name(formats[i].format),
                     formats[i].h_res, formats[i].v_res,
                     DWFRAMEINTERVAL_TO_FPS(formats[i].default_interval));
        }
    } else {
        ESP_LOGW(TAG, "Failed to read camera format list: %s", esp_err_to_name(ret));
    }

    free(formats);
}

static void usb_host_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t event_flags = 0;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));

        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
    }
}

static void return_queued_frames(uvc_host_stream_hdl_t stream)
{
    uvc_host_frame_t *frame = NULL;

    while (xQueueReceive(s_frame_queue, &frame, 0) == pdPASS) {
        uvc_host_frame_return(stream, frame);
    }
}

static void preview_stream(uvc_host_stream_hdl_t stream)
{
    uint32_t frame_count = 0;
    int64_t start_us = esp_timer_get_time();

    while (s_streaming) {
        uvc_host_frame_t *frame = NULL;
        if (xQueueReceive(s_frame_queue, &frame, pdMS_TO_TICKS(5000)) != pdPASS) {
            ESP_LOGW(TAG, "No frame received");
            continue;
        }

        esp_err_t ret = draw_camera_frame(frame);
        uvc_host_frame_return(stream, frame);

        vTaskDelay(1);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Draw frame failed: %s", esp_err_to_name(ret));
            continue;
        }

        frame_count++;
        int64_t now_us = esp_timer_get_time();
        if (now_us - start_us >= 3000000) {
            double fps = frame_count * 1000000.0 / (double)(now_us - start_us);
            ESP_LOGI(TAG, "LCD preview FPS: %.1f", fps);
            frame_count = 0;
            start_us = now_us;
        }
    }
}

static esp_err_t open_stream(uvc_host_stream_hdl_t *stream)
{
    uvc_host_stream_config_t config = s_stream_config;

    for (size_t i = 0; i < sizeof(s_preferred_formats) / sizeof(s_preferred_formats[0]); i++) {
        config.vs_format = s_preferred_formats[i];
        log_stream_format("Opening UVC stream", &config.vs_format);

        esp_err_t ret = uvc_host_stream_open(&config, pdMS_TO_TICKS(5000), stream);
        if (ret == ESP_OK) {
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Open candidate %u failed: %s", (unsigned)i, esp_err_to_name(ret));
    }

    return ESP_ERR_NOT_FOUND;
}

static void frame_task(void *arg)
{
    (void)arg;

    while (true) {
        uvc_host_stream_hdl_t stream = NULL;
        esp_err_t ret = open_stream(&stream);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "No matching UVC stream, retrying");
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        uvc_host_stream_format_t selected = {0};
        if (uvc_host_stream_format_get(stream, &selected) == ESP_OK) {
            log_stream_format("Selected UVC stream", &selected);
        }

        s_streaming = true;
        ret = uvc_host_stream_start(stream);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Stream start failed: %s", esp_err_to_name(ret));
            s_streaming = false;
            uvc_host_stream_close(stream);
            lcd_show_no_camera_message();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        preview_stream(stream);

        ESP_LOGI(TAG, "Stopping UVC stream");
        uvc_host_stream_stop(stream);
        return_queued_frames(stream);
        uvc_host_stream_close(stream);
        lcd_show_no_camera_message();
    }
}

static esp_err_t app_start(void)
{
    esp_err_t ret = ESP_OK;
    bool lcd_initialized = false;
    bool lvgl_initialized = false;
    bool host_installed = false;
    bool uvc_installed = false;
    TaskHandle_t usb_task_handle = NULL;
    TaskHandle_t frame_task_handle = NULL;

    ESP_LOGI(TAG, "Initializing LCD");
    ret = lcd_init();
    if (ret != ESP_OK) {
        goto cleanup;
    }
    lcd_initialized = true;
    /* Use LVGL only for Montserrat 14; camera frames still draw directly. */
    lv_init();
    lvgl_initialized = true;
    lcd_show_no_camera_message();

    const size_t rgb565_size = CAMERA_H_RES * CAMERA_V_RES * sizeof(uint16_t);
    s_rgb565_buf = heap_caps_aligned_alloc(16, rgb565_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_rgb565_buf) {
        ESP_LOGW(TAG, "Internal RGB buffer allocation failed, using PSRAM");
        s_rgb565_buf = heap_caps_aligned_alloc(16, rgb565_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!s_rgb565_buf) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    s_frame_queue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(uvc_host_frame_t *));
    if (!s_frame_queue) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Installing USB host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };
    ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    host_installed = true;

    if (xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, NULL,
                                USB_TASK_PRIORITY, &usb_task_handle,
                                USB_TASK_CORE) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ESP_LOGI(TAG, "Installing UVC host");
    const uvc_host_driver_config_t uvc_config = {
        .driver_task_stack_size = 6 * 1024,
        .driver_task_priority = USB_TASK_PRIORITY + 1,
        .xCoreID = USB_TASK_CORE,
        .create_background_task = true,
        .event_cb = camera_driver_cb,
    };
    ret = uvc_host_install(&uvc_config);
    if (ret != ESP_OK) {
        goto cleanup;
    }
    uvc_installed = true;

    if (xTaskCreatePinnedToCore(frame_task, "frame_hdl", FRAME_TASK_STACK_SIZE, NULL,
                                FRAME_TASK_PRIORITY, &frame_task_handle,
                                FRAME_TASK_CORE) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    return ESP_OK;

cleanup:
    if (frame_task_handle) {
        vTaskDelete(frame_task_handle);
    }
    if (uvc_installed) {
        uvc_host_uninstall();
    }
    if (usb_task_handle) {
        vTaskDelete(usb_task_handle);
    }
    if (host_installed) {
        usb_host_uninstall();
    }
    if (s_frame_queue) {
        vQueueDelete(s_frame_queue);
        s_frame_queue = NULL;
    }
    free(s_rgb565_buf);
    s_rgb565_buf = NULL;
    if (lvgl_initialized) {
        lv_deinit();
    }
    if (lcd_initialized) {
        lcd_deinit();
    }
    return ret;
}

void app_main(void)
{
    ESP_ERROR_CHECK(app_start());
}
