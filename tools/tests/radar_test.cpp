#include "radar_model.hpp"
#include "radar_raster.hpp"
#include "radar_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>
using namespace el_sistema;

static RadarObservation wifi(uint16_t id, int rssi = -70)
{
    RadarObservation o; o.address = {2,0,0,0,uint8_t(id>>8),uint8_t(id)};
    o.rssi = rssi; o.name = "Misma red"; o.channel = 6; o.security = 3; return o;
}
int main(int argc, char** argv)
{
    for (const auto screen : {std::array<int,2>{1280,720}, std::array<int,2>{720,1280}}) {
        const auto layout = radar_layout(screen[0],screen[1]);
        assert(layout.diameter == 720);
        assert(layout.controls_x+layout.controls_width <= screen[0]);
        assert(layout.title_y+48 <= layout.filters_y);
        assert(layout.filters_y+48 <= layout.legend_y);
        assert(layout.actions_y+48 <= layout.legend_y);
        assert(layout.legend_y+48 <= layout.status_y);
        assert(layout.status_y+(layout.portrait ? 28 : 48) <= layout.detail_y);
        assert(layout.detail_y+layout.detail_height <= layout.list_y);
        assert(layout.list_y+layout.list_height <= layout.pager_y);
        assert(layout.pager_y+48 <= screen[1]);
        assert(layout.portrait ? layout.title_y >= layout.diameter : layout.controls_x >= layout.diameter);
    }
    RadarModel model;
    auto session = model.begin();
    assert(model.observe(session,wifi(1),0));
    assert(model.observe(session,wifi(2),0));
    assert(model.snapshot().emitters.size() == 2); // BSSID, not SSID.
    assert(model.observe(session,wifi(1,-50),1000));
    auto s = model.snapshot();
    assert(std::abs(s.emitters.front().smoothed_rssi+65) < .01f);
    const auto selected = s.emitters.front().id;
    model.select(selected);
    RadarObservation ble = wifi(1); ble.kind = RadarKind::Ble; ble.name.clear();
    assert(model.observe(session,ble,2000));
    ble.address_type = 1; assert(model.observe(session,ble,2000));
    assert(model.snapshot().emitters.size() == 4);
    assert(radar_address(ble) == "02:00:00:00:00:01");
    model.expire(62000);
    assert(model.snapshot().emitters.size() == 1); // selected retained.
    for (int i=3; i<160; ++i) model.observe(session,wifi(i),63000+i);
    s = model.snapshot(); assert(s.emitters.size() == 128 && s.limited);
    bool preserved = false; for (const auto& e : s.emitters) preserved |= e.id == selected;
    assert(preserved);
    model.pause(true); assert(!model.observe(session,wifi(900),64000));
    model.pause(false); assert(!model.observe(session,wifi(900),64000));
    session = model.snapshot().session;
    assert(model.observe(session,wifi(900),64000));
    model.end(); assert(!model.observe(session,wifi(901),65000));
    session = model.begin(); assert(!model.observe(session-1,wifi(901),65000));
    assert(!model.observe(session,wifi(901,127),65000));

    RadarModel trends; session = trends.begin();
    for (int sec=0; sec<=16; ++sec) trends.observe(session,wifi(1,sec < 7 ? -90 : -40),sec*1000);
    assert(trends.snapshot().emitters[0].trend == RadarTrend::Increasing);
    for (int sec=17; sec<=34; ++sec) trends.observe(session,wifi(1,-40),sec*1000);
    assert(trends.snapshot().emitters[0].trend == RadarTrend::Stable);
    for (int sec=35; sec<=46; ++sec) trends.observe(session,wifi(1,-95),sec*1000);
    assert(trends.snapshot().emitters[0].trend == RadarTrend::Decreasing);
    for (int sec=47; sec<=150; ++sec) trends.observe(session,wifi(1,-80),sec*1000);
    assert(trends.snapshot().emitters[0].history_count <= 61);
    assert(trends.snapshot().emitters[0].history[0].time_ms >= 90000);
    const uint8_t advertisement[] = {3,8,'a','b',5,9,'M','a','c','!',0};
    assert(radar_ble_name(advertisement,sizeof(advertisement)) == "Mac!");
    const uint8_t malformed[] = {7,9,'x'};
    assert(radar_ble_name(malformed,sizeof(malformed)).empty());

    RadarModel drawing; session = drawing.begin();
    for (int i=1; i<=35; ++i) {
        auto o = wifi(i,-30-i*2); if (i%2) o.kind = RadarKind::Ble;
        drawing.observe(session,o,0);
    }
    s = drawing.snapshot(); drawing.select(s.emitters.back().id); s = drawing.snapshot();
    auto points = radar_points(s,0); assert(points.size() == 24);
    const auto again = radar_points(s,0);
    assert(points[0].x == again[0].x && points[0].y == again[0].y);
    assert(std::hypot(points[0].x,points[0].y) < std::hypot(points.back().x,points.back().y));
    assert(!radar_hit_test(points,points[0].x,points[0].y,.02f).empty());
    drawing.filter(RadarFilter::Wifi);
    for (auto p : radar_points(drawing.snapshot(),15000)) assert(p.kind == RadarKind::Wifi && p.stale);
    std::vector<RadarPoint> overlap = {{1,0,0,RadarKind::Wifi,false},{2,0,0,RadarKind::Ble,false}};
    assert(radar_hit_test(overlap,0,0,.1f).size() == 2);
    constexpr int size = 720;
    RadarRaster raster;
    assert(!raster.init(721));
    assert(raster.init(size));
    std::vector<uint16_t> pixels(size*size+2,0xA55A);
    raster.draw(pixels.data()+1,{},0,0);
    const uint16_t green = ((208>>2)<<5)|(112>>3);
    // Flat green on either side of the former reflection, black grid, no gradient.
    assert(pixels[1+210*size+220] == green);
    assert(pixels[1+510*size+500] == green);
    assert(pixels[1+210*size+216] == 0);
    assert(pixels[1+216*size+220] == 0);
    raster.draw(pixels.data()+1,points,s.selected_id,.5f);
    assert(pixels.front() == 0xA55A && pixels.back() == 0xA55A);
    assert(pixels[1] == 0); // circle corners stay black.
    std::array<std::vector<uint16_t>,2> frames = {std::vector<uint16_t>(size*size), std::vector<uint16_t>(size*size)};
    std::array<std::vector<RadarPoint>,2> past;
    std::array<float,2> angles {};
    std::vector<uint16_t> reference(size*size);
    for (int frame=0; frame<100; ++frame) {
        const int back = frame%2;
        auto moving = points;
        for (size_t i=0; i<moving.size(); ++i) {
            moving[i].x += .015f*std::sin(frame*.13f+i);
            moving[i].y += .015f*std::cos(frame*.13f+i);
            moving[i].stale = frame%7 == 0;
        }
        if (frame%3 == 0) moving.resize(moving.size()/2);
        const float angle = frame*.14f;
        const auto dirty = radar_dirty_regions(size,past[back],moving,angles[back],angle);
        assert(dirty.size() <= 12);
        if (frame<2) raster.draw(frames[back].data(),moving,frame%24,angle);
        else raster.draw_regions(frames[back].data(),moving,frame%24,angle,dirty);
        raster.draw(reference.data(),moving,frame%24,angle);
        assert(frames[back] == reference); // no trails, including overlap and removal.
        past[back] = moving; angles[back] = angle;
    }
    if (argc > 1) {
        std::ofstream file(argv[1],std::ios::binary); file << "P6\n" << size << ' ' << size << "\n255\n";
        for (size_t i=1; i<pixels.size()-1; ++i) {
            const auto p = pixels[i];
            const char rgb[] = {char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};
            file.write(rgb,3);
        }
    }
    std::puts("Radar: identidad, RSSI, tendencias, límites, sesiones, BLE, filtros, colisiones, layout y raster 720 px OK");
}
