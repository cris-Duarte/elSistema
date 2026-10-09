#pragma once
#include "radar_raster.hpp"
#include "M5GFX.h"
#include "lvgl.h"

namespace el_sistema {
class RadarRenderer {
public:
    bool init(lv_obj_t* image, int size);
    void draw(const std::vector<RadarPoint>& points, uint32_t selected, float sweep);
private:
    RadarRaster raster_;
    std::array<M5Canvas, 2> canvases_;
    int front_ = 0;
    lv_obj_t* image_ = nullptr;
    lv_image_dsc_t descriptor_ {};
    std::array<std::vector<RadarPoint>, 2> previous_;
    std::array<float, 2> previous_sweep_ {};
    std::array<bool, 2> initialized_ {};
};
} // namespace el_sistema
