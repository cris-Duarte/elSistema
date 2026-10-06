#pragma once

#include "home_scene.hpp"
#include "light_particle.hpp"
#include "M5GFX.h"
#include "lvgl.h"

namespace el_sistema {

// Double-buffered RAM canvas; LVGL remains the only physical flush owner.
class HomeRenderer {
public:
    bool init(lv_obj_t* image, int width, int height);
    void draw(const HomeScene& scene);

private:
    // Radius includes the glow: clearing and LVGL invalidation must erase the
    // entire halo, not only the physical grain, to avoid luminous trails.
    struct Dot { int x = 0, y = 0, radius = 0; };
    struct Frame {
        M5Canvas canvas;
        std::array<Dot, MAX_PARTICLES> dots{};
    };
    std::array<Frame, 2> frames_;
    std::array<std::array<LightSprite, LIGHT_MAX_RADIUS>, 4> light_sprites_;
    lv_image_dsc_t image_{};
    lv_obj_t* object_ = nullptr;
    int front_ = 0;
    int width_ = 0, height_ = 0;
};

}  // namespace el_sistema
