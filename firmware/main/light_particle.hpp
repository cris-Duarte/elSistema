#pragma once

#include "theme.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace el_sistema {

inline constexpr int LIGHT_MAX_RADIUS = 9;
inline constexpr int LIGHT_MAX_SIDE = LIGHT_MAX_RADIUS * 2 + 1;

inline constexpr uint16_t light_rgb565(uint32_t rgb)
{
    return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F);
}

// Add emitted light, rather than painting an opaque dark disc over neighbouring
// halos. RGB565 channel arithmetic avoids per-pixel floating point in frames.
inline uint16_t add_light(uint16_t background, uint16_t light, unsigned opacity = 255)
{
    if (opacity == 0 || light == 0) return background;
    unsigned r = light >> 11, g = (light >> 5) & 63, b = light & 31;
    if (opacity < 255) {
        r = (r * opacity + 127) / 255;
        g = (g * opacity + 127) / 255;
        b = (b * opacity + 127) / 255;
    }
    return (std::min(31u, (background >> 11) + r) << 11) |
           (std::min(63u, ((background >> 5) & 63) + g) << 5) |
           std::min(31u, (background & 31) + b);
}

struct LightSprite {
    std::array<uint16_t, LIGHT_MAX_SIDE * LIGHT_MAX_SIDE> pixels{};
    int radius = 0;

    void build(int size, uint32_t tint)
    {
        radius = std::clamp(size, 1, LIGHT_MAX_RADIUS);
        pixels.fill(0);
        const int side = radius * 2 + 1;
        const float edge = std::exp(-5.0f);
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                const float squared = static_cast<float>(x * x + y * y);
                if (squared >= radius * radius) continue;
                const float intensity = (std::exp(-5.0f * squared / (radius * radius)) - edge) / (1 - edge);
                const float white = std::exp(-squared / 2.0f);
                uint32_t rgb = 0;
                for (int shift : {0, 8, 16}) {
                    const int hue = (tint >> shift) & 255;
                    const int core = (theme::HomeLightCore >> shift) & 255;
                    rgb |= static_cast<uint32_t>((hue + (core - hue) * white) * intensity) << shift;
                }
                pixels[(y + radius) * side + x + radius] = light_rgb565(rgb);
            }
        }
    }
};

} // namespace el_sistema
