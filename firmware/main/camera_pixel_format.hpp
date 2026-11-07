#pragma once
#include "driver/jpeg_types.h"

namespace el_sistema {
// IDF 5.4.2's JPEG RGB565 API uses RGB/BGR to select byte endianness:
// RGB enables DMA BYTE2_0_1 (big endian); BGR leaves native little endian.
// LVGL RGB565 is a native uint16_t on ESP32-P4. This is NOT a red/blue swap
// and must not change the global LCD, PPA or LVGL flush configuration.
inline constexpr jpeg_dec_rgb_element_order_t CAMERA_JPEG_PIXEL_ORDER = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
inline constexpr jpeg_dec_output_format_t CAMERA_JPEG_PIXEL_FORMAT = JPEG_DECODE_OUT_FORMAT_RGB565;
}
