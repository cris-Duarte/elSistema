#include "hourglass_raster.hpp"
#include "theme.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace el_sistema {
namespace {
uint32_t random_pixel(int x, int y)
{
    uint32_t value = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}
}
bool HourglassRaster::init(int width, int height)
{
    if (width < 100 || width > 720 || height < 200 || height > 1200) return false;
    width_ = width; height_ = height;
    scale_x_ = width * 0.67f;
    scale_y_ = (height - 100) * 0.5f;
    middle_ = height * 0.5f;
    lights_[0].build(2, theme::HourglassAmber);
    lights_[1].build(2, theme::HourglassGold);
    lights_[2].build(3, theme::HourglassCore);
    // Bake bulk-grain brightness once, avoiding per-pixel opacity arithmetic.
    for (size_t i = 0; i < 2; ++i) {
        for (uint16_t& pixel : lights_[i].pixels) pixel = add_light(0, pixel, i == 0 ? 145 : 220);
    }
    for (int y = 0; y < height; ++y) {
        const float local_y = (y - middle_) / scale_y_;
        half_widths_[y] = std::abs(local_y) <= 1 ? HourglassScene::half_width(local_y) : 0;
    }
    return true;
}
void HourglassRaster::point(uint16_t* pixels, int x, int y, const LightSprite& light, unsigned opacity) const
{
    const int radius = light.radius, side = radius * 2 + 1;
    const int top = std::max(-radius, -y), bottom = std::min(radius, height_ - y - 1);
    for (int dy = top; dy <= bottom; ++dy) {
        const int py = y + dy;
        const int half = static_cast<int>(half_widths_[py] * scale_x_);
        const int left = std::max(-radius, width_ / 2 - half - x);
        const int right = std::min(radius, width_ / 2 + half - x);
        for (int dx = left; dx <= right; ++dx) {
            const int px = x + dx;
            if (px < 0 || px >= width_) continue;
            const uint16_t light_pixel = light.pixels[(dy + radius) * side + dx + radius];
            if (!light_pixel) continue;
            const size_t index = py * width_ + px;
            pixels[index] = add_light(pixels[index], light_pixel, opacity);
        }
    }
}
void HourglassRaster::line(uint16_t* pixels, int x0, int y0, int x1, int y1, uint16_t tint) const
{
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        if (x0 >= 0 && x0 < width_ && y0 >= 0 && y0 < height_) pixels[y0 * width_ + x0] = tint;
        if (x0 == x1 && y0 == y1) break;
        const int next = error * 2;
        if (next >= dy) { error += dy; x0 += sx; }
        if (next <= dx) { error += dx; y0 += sy; }
    }
}
void HourglassRaster::draw(uint16_t* pixels, const HourglassSnapshot& state) const
{
    if (!pixels || !width_) return;
    std::memset(pixels, 0, size_t(width_) * height_ * sizeof(uint16_t));
    const int center = width_ / 2;
    const int top = static_cast<int>(middle_ - scale_y_);
    const int bottom = static_cast<int>(middle_ + scale_y_);
    const uint16_t glass = light_rgb565(theme::HourglassGlass);
    const uint16_t reflection = light_rgb565(theme::HourglassReflection);
    const uint16_t frame = light_rgb565(theme::HourglassFrame);
    const uint16_t gold = light_rgb565(theme::HourglassGold);
    for (int y = top; y <= bottom && y < height_; ++y) {
        const int half = static_cast<int>(half_widths_[y] * scale_x_);
        for (int stroke = 0; stroke < 2; ++stroke) {
            line(pixels, center - half - stroke - 3, y, center - half - stroke - 3, y, glass);
            line(pixels, center + half + stroke + 3, y, center + half + stroke + 3, y, glass);
        }
        if (std::abs(y - middle_) > scale_y_ * 0.13f && std::abs(y - middle_) < scale_y_ * 0.80f) {
            line(pixels, center - half + 4, y, center - half + 4, y, reflection);
            line(pixels, center + half - 4, y, center + half - 4, y, glass);
        }
    }
    const int rim = static_cast<int>(HourglassScene::half_width(1) * scale_x_) + 12;
    for (int stroke = 0; stroke < 4; ++stroke) {
        line(pixels, center - rim, top - stroke, center + rim, top - stroke, frame);
        line(pixels, center - rim, bottom + stroke, center + rim, bottom + stroke, frame);
    }
    line(pixels, center - rim, top - 5, center + rim, top - 5, gold);
    line(pixels, center - rim, bottom + 5, center + rim, bottom + 5, gold);

    // Dense, stable grain texture. The threshold is mass-based, not elapsed/T.
    // Cores are separated by dark interstices, with small additive halos.
    const float visual_seconds = (state.elapsed_us % 60000000) / 1000000.0f;
    for (int y = top + 2; y < bottom; y += 4) {
        const float world_y = (y - middle_) / scale_y_;
        const bool upper = world_y < 0;
        if ((upper ? state.upper_mass : state.lower_mass) <= 0) continue;
        const SandProfile& profile = upper ? state.upper : state.lower;
        const float local_y = std::abs(world_y);
        const int half = static_cast<int>(half_widths_[y] * scale_x_) - 3;
        for (int x = center - half + (y % 8 ? 2 : 0); x < center + half; x += 4) {
            const float world_x = (x - center) / scale_x_;
            const float depth = profile.score(world_x, local_y) - profile.threshold;
            if (depth < 0) continue;
            const uint32_t random = random_pixel(x, y);
            const int jitter_x = int(random % 3) - 1, jitter_y = int((random >> 4) % 3) - 1;
            point(pixels, x + jitter_x, y + jitter_y, lights_[(random >> 12) % 2]);
            // A few surface grains slide with the tilt, staying in their own
            // chamber. Their visible trails never cross the chamber boundary.
            if (depth < 0.035f && random % 9 == 0 && state.state == HourglassState::Running) {
                const float travel = std::fmod(visual_seconds * 3 + (random % 37) * 0.1f, 1.0f);
                const int px = x + static_cast<int>(state.gravity_x * travel * 8);
                const int py = y + static_cast<int>(state.gravity_y * travel * 5);
                if ((py < middle_) == upper) point(pixels, px, py, lights_[2], 150);
            }
        }
    }
    if (state.stream_life > 0 && state.flow_direction != 0) {
        // Moving light grains represent the continuous mass transferred by the
        // engine; their number does not quantize a two-hour physical interval.
        const int direction = state.flow_direction;
        for (int i = 0; i < 30; ++i) {
            const float age = std::fmod(visual_seconds * 2.3f + i / 30.0f, 1.0f);
            const float world_y = direction * (0.015f + 0.92f * age * age);
            const float world_x = state.gravity_x * age * age * 0.22f + (int(random_pixel(i, 3) % 5) - 2) * 0.005f;
            const SandProfile& destination = direction > 0 ? state.lower : state.upper;
            if (destination.score(world_x, std::abs(world_y)) >= destination.threshold) continue;
            const int px = center + static_cast<int>(world_x * scale_x_);
            const int py = static_cast<int>(middle_ + world_y * scale_y_);
            point(pixels, px, py, lights_[i % 8 == 0 ? 2 : 1], 230);
        }
    }
}
} // namespace el_sistema
