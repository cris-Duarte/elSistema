#include "camera_pixel_format.hpp"
#include <cstdio>

using namespace el_sistema;
// Compile against the real IDF types, not a mock with conveniently chosen enum
// values. This guards the decoder/LVGL contract; it does NOT run P4 hardware.
static_assert(CAMERA_JPEG_PIXEL_FORMAT == JPEG_DECODE_OUT_FORMAT_RGB565);
static_assert(CAMERA_JPEG_PIXEL_ORDER == JPEG_DEC_RGB_ELEMENT_ORDER_BGR);
static_assert(CAMERA_JPEG_PIXEL_ORDER != JPEG_DEC_RGB_ELEMENT_ORDER_RGB);
int main()
{
    std::puts("Cámara: contrato JPEG RGB565 little-endian/LVGL OK (tipos SDK reales; color físico pendiente).");
}
