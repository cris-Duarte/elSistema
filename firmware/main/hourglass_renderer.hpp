#pragma once
#include "hourglass_raster.hpp"
#include "M5GFX.h"
#include "lvgl.h"

namespace el_sistema {
class HourglassRenderer {
public:
    bool init(lv_obj_t* image, int width = 480, int height = 1040);
    void draw(const HourglassSnapshot& snapshot);
private:
    std::array<M5Canvas, 2> canvases_;
    HourglassRaster raster_;
    lv_image_dsc_t descriptor_{};
    lv_obj_t* image_ = nullptr;
    int front_ = 0;
};
} // namespace el_sistema
