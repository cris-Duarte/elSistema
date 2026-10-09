#include "hourglass_renderer.hpp"

namespace el_sistema {
bool HourglassRenderer::init(lv_obj_t* image, int width, int height)
{
    if (!raster_.init(width, height)) return false;
    image_ = image;
    for (auto& canvas : canvases_) {
        canvas.setPsram(true);
        canvas.setColorDepth(lgfx::rgb565_nonswapped);
        if (!canvas.createSprite(width, height)) return false;
        canvas.fillScreen(0);
    }
    descriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
    descriptor_.header.w = width;
    descriptor_.header.h = height;
    descriptor_.header.stride = width * 2;
    descriptor_.data_size = width * height * 2;
    descriptor_.data = static_cast<const uint8_t*>(canvases_[front_].getBuffer());
    lv_image_set_src(image_, &descriptor_);
    return true;
}
void HourglassRenderer::draw(const HourglassSnapshot& snapshot)
{
    const int back = 1 - front_;
    raster_.draw(static_cast<uint16_t*>(canvases_[back].getBuffer()), snapshot);
    front_ = back;
    descriptor_.data = static_cast<const uint8_t*>(canvases_[front_].getBuffer());
    lv_image_cache_drop(&descriptor_);
    lv_obj_invalidate(image_);
}
} // namespace el_sistema
