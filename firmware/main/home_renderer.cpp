#include "home_renderer.hpp"
#include "theme.hpp"

#include <algorithm>
#include <cmath>

namespace el_sistema {
namespace {

constexpr uint32_t kParticleTints[] = {theme::HomeParticleCyan, theme::HomeParticleLime,
                                       theme::HomeParticleRose, theme::HomeParticleLavender};

} // namespace

bool HomeRenderer::init(lv_obj_t* image, int width, int height)
{
    if (width <= 0 || height <= 0 || width > 1280 || height > 1280) return false;
    object_ = image;
    width_ = width;
    height_ = height;
    for (size_t tint = 0; tint < light_sprites_.size(); ++tint) {
        for (int radius = 1; radius <= LIGHT_MAX_RADIUS; ++radius) {
            light_sprites_[tint][radius - 1].build(radius, kParticleTints[tint]);
        }
    }
    for (Frame& frame : frames_) {
        frame.canvas.setPsram(true);
        frame.canvas.setColorDepth(lgfx::rgb565_nonswapped);
        if (frame.canvas.createSprite(width, height) == nullptr) return false;
        frame.canvas.fillScreen(theme::HomeBackground);
    }
    image_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_.header.cf = LV_COLOR_FORMAT_RGB565;
    image_.header.w = width;
    image_.header.h = height;
    image_.header.stride = width * 2;
    image_.data_size = width * height * 2;
    image_.data = static_cast<const uint8_t*>(frames_[front_].canvas.getBuffer());
    lv_image_set_src(object_, &image_);
    return true;
}

void HomeRenderer::draw(const HomeScene& scene)
{
    const int back = 1 - front_;
    Frame& frame = frames_[back];
    for (const Dot& dot : frame.dots) {
        if (dot.radius > 0) frame.canvas.fillRect(dot.x - dot.radius, dot.y - dot.radius,
                                                 dot.radius * 2 + 1, dot.radius * 2 + 1,
                                                 theme::HomeBackground);
    }
    const auto previous = frames_[front_].dots;
    frame.dots.fill({});
    auto* pixels = static_cast<uint16_t*>(frame.canvas.getBuffer());

    const auto paint = [&](size_t index, const Particle& particle) {
        Dot& dot = frame.dots[index];
        dot.x = static_cast<int>(std::lround(particle.x));
        dot.y = static_cast<int>(std::lround(particle.y));
        dot.radius = std::clamp(static_cast<int>(std::lround(particle.radius)) + 3, 1, LIGHT_MAX_RADIUS);
        size_t tint = 0;
        for (size_t color = 0; color < std::size(kParticleTints); ++color) {
            if (particle.color == kParticleTints[color]) { tint = color; break; }
        }
        const LightSprite& sprite = light_sprites_[tint % light_sprites_.size()][dot.radius - 1];
        const int side = dot.radius * 2 + 1;
        const int left = std::max(-dot.radius, -dot.x);
        const int right = std::min(dot.radius, width_ - dot.x - 1);
        const int top = std::max(-dot.radius, -dot.y);
        const int bottom = std::min(dot.radius, height_ - dot.y - 1);
        for (int y = top; y <= bottom; ++y) {
            uint16_t* row = pixels + (dot.y + y) * width_;
            for (int x = left; x <= right; ++x) {
                const uint16_t light = sprite.pixels[(y + dot.radius) * side + x + dot.radius];
                if (light != 0) {
                    row[dot.x + x] = add_light(row[dot.x + x], light,
                                               static_cast<unsigned>(std::lround(scene.visibility() * 255.0f)));
                }
            }
        }
    };

    for (size_t index = 0; index < scene.active_count(); ++index) {
        const Particle& particle = scene.particles()[index];
        if (particle.active) paint(index, particle);
    }

    front_ = back;
    image_.data = static_cast<const uint8_t*>(frame.canvas.getBuffer());
    lv_image_cache_drop(&image_);

    constexpr size_t kMaxDirty = 20;
    std::array<lv_area_t, kMaxDirty + 1> regions{};
    size_t region_count = 0;
    const auto unite = [](lv_area_t a, const lv_area_t& b) {
        a.x1 = std::min(a.x1, b.x1); a.y1 = std::min(a.y1, b.y1);
        a.x2 = std::max(a.x2, b.x2); a.y2 = std::max(a.y2, b.y2);
        return a;
    };
    const auto area = [](const lv_area_t& a) -> int64_t {
        return static_cast<int64_t>(a.x2 - a.x1 + 1) * (a.y2 - a.y1 + 1);
    };
    const auto mark = [&](lv_area_t& dirty, const Dot& dot) {
        if (dot.radius < 1) return;
        dirty.x1 = std::min<int>(dirty.x1, std::max(0, dot.x - dot.radius));
        dirty.y1 = std::min<int>(dirty.y1, std::max(0, dot.y - dot.radius));
        dirty.x2 = std::max<int>(dirty.x2, std::min(width_ - 1, dot.x + dot.radius));
        dirty.y2 = std::max<int>(dirty.y2, std::min(height_ - 1, dot.y + dot.radius));
    };
    for (size_t index = 0; index < frame.dots.size(); ++index) {
        const Dot& before = previous[index];
        const Dot& after = frame.dots[index];
        if (before.x == after.x && before.y == after.y && before.radius == after.radius) continue;
        lv_area_t dirty{width_, height_, -1, -1};
        mark(dirty, before);
        mark(dirty, after);
        if (dirty.x1 > dirty.x2 || dirty.y1 > dirty.y2) continue;
        regions[region_count++] = dirty;
        while (region_count > 1) {
            size_t first = 0, second = 1;
            int64_t best_cost = INT64_MAX;
            bool cheap_merge = false;
            for (size_t i = 0; i + 1 < region_count; ++i) {
                for (size_t j = i + 1; j < region_count; ++j) {
                    const int64_t combined = area(unite(regions[i], regions[j]));
                    const int64_t separate = area(regions[i]) + area(regions[j]);
                    const int64_t cost = combined - separate;
                    if (cost < best_cost) {
                        best_cost = cost;
                        first = i;
                        second = j;
                        cheap_merge = combined * 4 <= separate * 5;
                    }
                }
            }
            if (!cheap_merge && region_count <= kMaxDirty) break;
            regions[first] = unite(regions[first], regions[second]);
            regions[second] = regions[--region_count];
        }
    }
    for (size_t i = 0; i < region_count; ++i) lv_obj_invalidate_area(object_, &regions[i]);
}

} // namespace el_sistema
