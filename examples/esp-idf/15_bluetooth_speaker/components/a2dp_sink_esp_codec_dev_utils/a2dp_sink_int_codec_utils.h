/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef __A2DP_SINK_INT_CODEC_UTILS_H__
#define __A2DP_SINK_INT_CODEC_UTILS_H__

#include <stdint.h>

void bt_a2d_evt_int_codec_hdl(uint16_t event, void *param);
void bt_a2d_data_hdl(const uint8_t *data, uint32_t len);

#endif /* __A2DP_SINK_INT_CODEC_UTILS_H__ */
