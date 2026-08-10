/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_ui_create(void);
void app_ui_start(void);
void app_ui_notify_video_frame(void);
bool app_ui_turbo_enabled(void);

#ifdef __cplusplus
}
#endif
