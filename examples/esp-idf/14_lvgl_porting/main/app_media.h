/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MEDIA_RGB565_BYTES_PER_PIXEL    2
#define APP_MEDIA_PHOTO_W                   150
#define APP_MEDIA_PHOTO_H                   96
#define APP_MEDIA_ANIM_W                    80
#define APP_MEDIA_ANIM_H                    80
#define APP_MEDIA_ANIM_FRAME_COUNT          8
#define APP_MEDIA_VIDEO_W                   304
#define APP_MEDIA_VIDEO_H                   152

esp_err_t app_media_init(void);
const lv_image_dsc_t *app_media_photo(void);
const lv_image_dsc_t *app_media_anim_frame(size_t frame);
uint16_t *app_media_video_buffer(void);
void app_media_render_video_frame(uint32_t frame);

#ifdef __cplusplus
}
#endif
