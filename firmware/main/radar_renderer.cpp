#include "radar_renderer.hpp"
namespace el_sistema {
bool RadarRenderer::init(lv_obj_t* image, int size)
{
    if (!raster_.init(size)) return false;
    image_ = image;
    for (auto& canvas : canvases_) {
        canvas.setPsram(true); canvas.setColorDepth(lgfx::rgb565_nonswapped);
        if (!canvas.createSprite(size,size)) return false;
        canvas.fillScreen(0);
    }
    descriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
    descriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
    descriptor_.header.w = descriptor_.header.h = size;
    descriptor_.header.stride = size*2; descriptor_.data_size = size*size*2;
    descriptor_.data = static_cast<const uint8_t*>(canvases_[front_].getBuffer());
    lv_image_set_src(image_, &descriptor_); return true;
}
void RadarRenderer::draw(const std::vector<RadarPoint>& points, uint32_t selected, float sweep)
{
    const int back = 1-front_;
    // Restore this back-buffer against its OWN two-frame-old contents, while
    // invalidate against the frame currently displayed by LVGL.
    auto* pixels = static_cast<uint16_t*>(canvases_[back].getBuffer());
    if (!initialized_[back]) raster_.draw(pixels,points,selected,sweep);
    else raster_.draw_regions(pixels,points,selected,sweep,
                             radar_dirty_regions(raster_.size(),previous_[back],points,previous_sweep_[back],sweep));
    const bool full = !initialized_[front_];
    const auto dirty = full ? std::vector<RadarRect>{} :
        radar_dirty_regions(raster_.size(),previous_[front_],points,previous_sweep_[front_],sweep);
    previous_[back] = points; previous_sweep_[back] = sweep; initialized_[back] = true;
    front_ = back;
    descriptor_.data = static_cast<const uint8_t*>(canvases_[front_].getBuffer());
    lv_image_cache_drop(&descriptor_);
    if (full) lv_obj_invalidate(image_);
    else {
        lv_area_t origin; lv_obj_get_coords(image_,&origin);
        for (const auto& r : dirty) {
            const lv_area_t area {origin.x1+r.x1,origin.y1+r.y1,origin.x1+r.x2,origin.y1+r.y2};
            lv_obj_invalidate_area(image_,&area);
        }
    }
}
} // namespace el_sistema
