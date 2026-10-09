#pragma once

#include "hourglass_model.hpp"
#include "light_particle.hpp"

namespace el_sistema {
// Portable RGB565 compositor: also used by the desktop visual/physics tests.
// Pixels live in the renderer's PSRAM canvases, never in this model.
class HourglassRaster {
public:
    bool init(int width, int height);
    void draw(uint16_t* pixels, const HourglassSnapshot& state) const;
private:
    void point(uint16_t* pixels, int x, int y, const LightSprite& light, unsigned opacity = 255) const;
    void line(uint16_t* pixels, int x0, int y0, int x1, int y1, uint16_t color) const;
    int width_ = 0, height_ = 0;
    float scale_x_ = 1, scale_y_ = 1, middle_ = 0;
    std::array<float, 1200> half_widths_{};
    std::array<LightSprite, 3> lights_;
};
} // namespace el_sistema
