#include "radar_raster.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace el_sistema {
namespace {
uint16_t rgb(int r, int g, int b) { return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3); }
RadarRect unite(const RadarRect& a, const RadarRect& b)
{ return {std::min(a.x1,b.x1),std::min(a.y1,b.y1),std::max(a.x2,b.x2),std::max(a.y2,b.y2)}; }
int area(const RadarRect& a) { return (a.x2-a.x1+1)*(a.y2-a.y1+1); }
}
std::vector<RadarRect> radar_dirty_regions(int size, const std::vector<RadarPoint>& previous,
                                         const std::vector<RadarPoint>& current, float old_sweep, float sweep)
{
    std::vector<RadarRect> regions;
    const int center = size/2, radius = center-22;
    const auto add = [&](RadarRect r) {
        r.x1 = std::clamp(r.x1,0,size-1); r.x2 = std::clamp(r.x2,0,size-1);
        r.y1 = std::clamp(r.y1,0,size-1); r.y2 = std::clamp(r.y2,0,size-1);
        for (auto& existing : regions) {
            const auto merged = unite(existing,r);
            if (area(merged) <= area(existing)+area(r)+64) { existing = merged; return; }
        }
        regions.push_back(r);
    };
    for (float angle : {old_sweep,sweep}) {
        const int x = center+int(std::cos(angle)*(radius-1));
        const int y = center+int(std::sin(angle)*(radius-1));
        add({std::min(x,center)-2,std::min(y,center)-2,std::max(x,center)+2,std::max(y,center)+2});
    }
    for (const auto* list : {&previous,&current}) for (const auto& p : *list) {
        const int x = center+int(p.x*radius), y = center+int(p.y*radius);
        add({x-15,y-15,x+15,y+15});
    }
    // Leave room in LVGL's invalidation list for chart, rows and statusbar.
    while (regions.size() > 12) {
        size_t first = 0, second = 1; int best = INT32_MAX;
        for (size_t i=0; i<regions.size(); ++i) for (size_t j=i+1; j<regions.size(); ++j) {
            const int cost = area(unite(regions[i],regions[j]))-area(regions[i])-area(regions[j]);
            if (cost < best) { best=cost; first=i; second=j; }
        }
        regions[first] = unite(regions[first],regions[second]);
        regions.erase(regions.begin()+second);
    }
    return regions;
}
bool RadarRaster::init(int size)
{
    if (size < 80 || size > 720) return false;
    size_ = size; background_.resize(size*size);
    for (int y = -15; y <= 15; ++y) for (int x = -15; x <= 15; ++x) {
        const size_t index = (y+15)*31+x+15;
        const auto cache = [](float distance) {
            GlowPixel pixel;
            pixel.alpha = static_cast<uint8_t>(std::clamp((15-distance)*25.5f,0.f,255.f));
            pixel.outline = distance >= 13 && distance < 14;
            return pixel;
        };
        wifi_glow_[index] = cache(std::hypot(float(x),float(y)));
        ble_glow_[index] = cache((std::abs(x)+std::abs(y))*.75f);
    }
    const int center = size/2, radius = center-6, step = std::max(20, size/10);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const float distance = std::hypot(float(x-center), float(y-center));
        uint16_t pixel = 0;
        if (distance < radius-13) {
            pixel = rgb(0, 208, 112);
            if (std::abs(x-center)%step <= 1 || std::abs(y-center)%step <= 1) pixel = 0;
        } else if (distance <= radius) {
            pixel = rgb(241, 238, 222);
            const float angle = std::atan2(float(y-center), float(x-center));
            if (distance > radius-9 && std::fabs(std::sin(angle*30)) < .10f) pixel = rgb(35, 43, 37);
        }
        background_[y*size+x] = pixel;
    }
    return true;
}
void RadarRaster::draw(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected, float sweep)
{
    if (!pixels || !size_) return;
    std::memcpy(pixels, background_.data(), background_.size()*2);
    paint(pixels,points,selected,sweep);
}
void RadarRaster::draw_regions(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected,
                              float sweep, const std::vector<RadarRect>& regions)
{
    if (!pixels || !size_) return;
    for (const auto& r : regions) for (int y=r.y1; y<=r.y2; ++y)
        std::memcpy(pixels+y*size_+r.x1, background_.data()+y*size_+r.x1, (r.x2-r.x1+1)*2);
    paint(pixels,points,selected,sweep);
}
void RadarRaster::paint(uint16_t* pixels, const std::vector<RadarPoint>& points, uint32_t selected, float sweep)
{
    const int c = size_/2, radius = c-22;
    const auto put = [&](int x, int y, uint16_t color) {
        if (x >= 0 && x < size_ && y >= 0 && y < size_ && (x-c)*(x-c)+(y-c)*(y-c) < radius*radius)
            pixels[y*size_+x] = color;
    };
    // Hoist trig out of the pixel loop: libm calls have errno side-effects and
    // the compiler cannot safely move hundreds of calls itself.
    const float sweep_x = std::cos(sweep), sweep_y = std::sin(sweep);
    for (int r = 10; r < radius; ++r) {
        const int x = c + int(sweep_x*r), y = c + int(sweep_y*r);
        put(x,y,rgb(113,239,167)); put(x+1,y,rgb(91,221,150));
    }
    for (const auto& point : points) {
        const int px = c+int(point.x*radius), py = c+int(point.y*radius);
        const bool wifi = point.kind == RadarKind::Wifi;
        const auto& glow = wifi ? wifi_glow_ : ble_glow_;
        for (int y = -15; y <= 15; ++y) for (int x = -15; x <= 15; ++x) {
            const auto& pixel = glow[(y+15)*31+x+15];
            if (!pixel.alpha && !(point.id == selected && pixel.outline)) continue;
            const int target_x = px+x, target_y = py+y;
            if (target_x < 0 || target_x >= size_ || target_y < 0 || target_y >= size_) continue;
            const uint16_t base = pixels[target_y*size_+target_x];
            const int alpha = point.stale ? pixel.alpha*77/255 : pixel.alpha;
            const int r = (base>>11)&31, g = (base>>5)&63, b = base&31;
            const int blended_r = r + (((wifi?31:12)-r)*alpha >> 8);
            const int blended_g = g + ((62-g)*alpha >> 8);
            const int blended_b = b + (((wifi?9:31)-b)*alpha >> 8);
            put(target_x,target_y,uint16_t((blended_r<<11)|(blended_g<<5)|blended_b));
            if (point.id == selected && pixel.outline) put(target_x,target_y,rgb(255,255,240));
        }
    }
    for (int y = -12; y <= 14; ++y) for (int x = -12; x <= 12; ++x)
        if (std::abs(x) <= (14-y)*.50f) put(c+x,c+y,rgb(255,108,48));
}
} // namespace el_sistema
