/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "app_media.h"

#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "app_media";

static uint16_t *s_photo_buf;
static uint16_t *s_anim_buf[APP_MEDIA_ANIM_FRAME_COUNT];
static uint16_t *s_video_buf;
static lv_image_dsc_t s_photo_dsc;
static lv_image_dsc_t s_anim_dsc[APP_MEDIA_ANIM_FRAME_COUNT];

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3));
}

static uint8_t clamp_u8(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint8_t)value;
}

static uint16_t blend_rgb565(uint16_t a, uint16_t b, uint8_t mix)
{
    uint8_t ar = (uint8_t)(((a >> 11) & 0x1f) * 255 / 31);
    uint8_t ag = (uint8_t)(((a >> 5) & 0x3f) * 255 / 63);
    uint8_t ab = (uint8_t)((a & 0x1f) * 255 / 31);
    uint8_t br = (uint8_t)(((b >> 11) & 0x1f) * 255 / 31);
    uint8_t bg = (uint8_t)(((b >> 5) & 0x3f) * 255 / 63);
    uint8_t bb = (uint8_t)((b & 0x1f) * 255 / 31);

    uint8_t red = (uint8_t)((ar * (255 - mix) + br * mix) / 255);
    uint8_t green = (uint8_t)((ag * (255 - mix) + bg * mix) / 255);
    uint8_t blue = (uint8_t)((ab * (255 - mix) + bb * mix) / 255);
    return rgb565(red, green, blue);
}

static void image_dsc_init(lv_image_dsc_t *dsc, const uint16_t *buf, uint16_t w, uint16_t h)
{
    memset(dsc, 0, sizeof(*dsc));
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.stride = w * APP_MEDIA_RGB565_BYTES_PER_PIXEL;
    dsc->data_size = (uint32_t)w * h * APP_MEDIA_RGB565_BYTES_PER_PIXEL;
    dsc->data = (const uint8_t *)buf;
}

static void *media_alloc(size_t bytes)
{
    void *ptr = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void generate_photo_asset(void)
{
    for (int y = 0; y < APP_MEDIA_PHOTO_H; y++) {
        for (int x = 0; x < APP_MEDIA_PHOTO_W; x++) {
            int dx = x - APP_MEDIA_PHOTO_W / 2;
            int dy = y - APP_MEDIA_PHOTO_H / 2;
            int dist = (dx * dx + dy * dy) / 64;
            uint8_t red = clamp_u8(24 + x * 190 / APP_MEDIA_PHOTO_W + (y % 19) * 2);
            uint8_t green = clamp_u8(70 + y * 150 / APP_MEDIA_PHOTO_H);
            uint8_t blue = clamp_u8(190 - x * 70 / APP_MEDIA_PHOTO_W + (dist % 58));
            uint16_t base = rgb565(red, green, blue);

            if (((x / 12) + (y / 12)) % 5 == 0) {
                base = blend_rgb565(base, rgb565(255, 255, 255), 58);
            }
            if (dist < 72) {
                base = blend_rgb565(base, rgb565(255, 88, 44), (uint8_t)(180 - dist * 2));
            }
            s_photo_buf[y * APP_MEDIA_PHOTO_W + x] = base;
        }
    }
}

static void generate_anim_assets(void)
{
    for (int frame = 0; frame < APP_MEDIA_ANIM_FRAME_COUNT; frame++) {
        uint16_t *buf = s_anim_buf[frame];
        for (int y = 0; y < APP_MEDIA_ANIM_H; y++) {
            for (int x = 0; x < APP_MEDIA_ANIM_W; x++) {
                int cx = APP_MEDIA_ANIM_W / 2;
                int cy = APP_MEDIA_ANIM_H / 2;
                int dx = x - cx;
                int dy = y - cy;
                int radius2 = dx * dx + dy * dy;
                int ring = ((radius2 / 42) + frame * 3) % 22;
                uint8_t red = clamp_u8(32 + frame * 22 + x * 70 / APP_MEDIA_ANIM_W);
                uint8_t green = clamp_u8(210 - ring * 5);
                uint8_t blue = clamp_u8(80 + y * 130 / APP_MEDIA_ANIM_H + frame * 9);
                uint16_t color = rgb565(red, green, blue);

                if (ring < 6) {
                    color = blend_rgb565(color, rgb565(255, 255, 255), 120);
                }
                if (radius2 > 35 * 35) {
                    color = blend_rgb565(color, rgb565(13, 18, 32), 190);
                }
                buf[y * APP_MEDIA_ANIM_W + x] = color;
            }
        }
    }
}

esp_err_t app_media_init(void)
{
    s_photo_buf = media_alloc(APP_MEDIA_PHOTO_W * APP_MEDIA_PHOTO_H * APP_MEDIA_RGB565_BYTES_PER_PIXEL);
    ESP_RETURN_ON_FALSE(s_photo_buf, ESP_ERR_NO_MEM, TAG, "photo buffer allocation failed");
    generate_photo_asset();
    image_dsc_init(&s_photo_dsc, s_photo_buf, APP_MEDIA_PHOTO_W, APP_MEDIA_PHOTO_H);

    for (int i = 0; i < APP_MEDIA_ANIM_FRAME_COUNT; i++) {
        s_anim_buf[i] = media_alloc(APP_MEDIA_ANIM_W * APP_MEDIA_ANIM_H * APP_MEDIA_RGB565_BYTES_PER_PIXEL);
        ESP_RETURN_ON_FALSE(s_anim_buf[i], ESP_ERR_NO_MEM, TAG, "anim buffer allocation failed");
    }
    generate_anim_assets();
    for (int i = 0; i < APP_MEDIA_ANIM_FRAME_COUNT; i++) {
        image_dsc_init(&s_anim_dsc[i], s_anim_buf[i], APP_MEDIA_ANIM_W, APP_MEDIA_ANIM_H);
    }

    s_video_buf = media_alloc(APP_MEDIA_VIDEO_W * APP_MEDIA_VIDEO_H * APP_MEDIA_RGB565_BYTES_PER_PIXEL);
    ESP_RETURN_ON_FALSE(s_video_buf, ESP_ERR_NO_MEM, TAG, "video buffer allocation failed");
    memset(s_video_buf, 0, APP_MEDIA_VIDEO_W * APP_MEDIA_VIDEO_H * APP_MEDIA_RGB565_BYTES_PER_PIXEL);
    return ESP_OK;
}

const lv_image_dsc_t *app_media_photo(void)
{
    return &s_photo_dsc;
}

const lv_image_dsc_t *app_media_anim_frame(size_t frame)
{
    if (frame >= APP_MEDIA_ANIM_FRAME_COUNT) {
        return NULL;
    }
    return &s_anim_dsc[frame];
}

uint16_t *app_media_video_buffer(void)
{
    return s_video_buf;
}

void app_media_render_video_frame(uint32_t frame)
{
    uint16_t color_a = rgb565(26, 38, 62);
    uint16_t color_b = rgb565(25, 166, 210);
    uint16_t color_c = rgb565(255, 109, 91);
    uint16_t color_d = rgb565(245, 211, 92);
    int orb_x = (int)((frame * 7) % (APP_MEDIA_VIDEO_W + 90)) - 45;
    int orb_y = APP_MEDIA_VIDEO_H / 2 + (int)((frame * 5) % 72) - 36;

    for (int y = 0; y < APP_MEDIA_VIDEO_H; y++) {
        uint16_t *row = s_video_buf + y * APP_MEDIA_VIDEO_W;
        for (int x = 0; x < APP_MEDIA_VIDEO_W; x++) {
            uint8_t mix = (uint8_t)((x * 255) / (APP_MEDIA_VIDEO_W - 1));
            uint16_t color = blend_rgb565(color_a, color_b, mix);

            int wave = (x + (int)frame * 4 + (y / 7) * 13) % 64;
            if (wave < 7) {
                color = blend_rgb565(color, color_d, 86);
            }

            int dx = x - orb_x;
            int dy = y - orb_y;
            int dist2 = dx * dx + dy * dy;
            if (dist2 < 44 * 44) {
                uint8_t orb_mix = (uint8_t)(210 - dist2 * 210 / (44 * 44));
                color = blend_rgb565(color, color_c, orb_mix);
            }

            if (((x + frame * 3) % 49) < 3 || ((y + frame * 2) % 43) < 2) {
                color = blend_rgb565(color, rgb565(255, 255, 255), 32);
            }
            row[x] = color;
        }
    }
}
