#pragma once
#include "radar_model.hpp"

namespace el_sistema {
struct RadarRect { int x1 = 0, y1 = 0, x2 = 0, y2 = 0; };
std::vector<RadarRect> radar_dirty_regions(int size, const std::vector<RadarPoint>& previous,
                                         const std::vector<RadarPoint>& current, float old_sweep, float sweep);
class RadarRaster {
public:
    bool init(int size);
    void draw(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected, float sweep);
    void draw_regions(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected,
                      float sweep, const std::vector<RadarRect>& regions);
    int size() const { return size_; }
private:
    void paint(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected, float sweep);
    int size_ = 0;
    std::vector<uint16_t> background_;
    struct GlowPixel { uint8_t alpha = 0; bool outline = false; };
    std::array<GlowPixel, 31*31> wifi_glow_ {}, ble_glow_ {};
};
} // namespace el_sistema
