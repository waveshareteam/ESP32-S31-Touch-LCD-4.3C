/*
 * SPDX-FileCopyrightText: 2026
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef MUSIC_UI_H
#define MUSIC_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t music_ui_start(void);
void music_ui_set_peer_connected(bool connected);
void music_ui_set_stream_state(const char *state);
void music_ui_set_codec_info(int sample_rate, int channel_count);
void music_ui_set_playback_status(uint8_t status);
void music_ui_set_play_position(uint32_t position_ms);
void music_ui_set_play_duration(uint32_t duration_ms);
void music_ui_set_metadata_attr(uint8_t attr_id, const uint8_t *text, size_t len);
void music_ui_clear_metadata(void);

#ifdef __cplusplus
}
#endif

#endif /* MUSIC_UI_H */
