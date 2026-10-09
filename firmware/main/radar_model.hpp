#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace el_sistema {

enum class RadarKind : uint8_t { Wifi, Ble };
enum class RadarFilter : uint8_t { All, Wifi, Ble };
enum class RadarTrend : uint8_t { Unknown, Stable, Increasing, Decreasing };
struct RadarObservation {
    RadarKind kind = RadarKind::Wifi;
    std::array<uint8_t, 6> address {};
    uint8_t address_type = 0;
    std::string name;
    int16_t rssi = -127;
    uint8_t channel = 0;
    uint8_t security = 0;
};
struct RadarSample { int64_t time_ms = 0; float rssi = -127; };
struct RadarEmitter {
    uint32_t id = 0;
    RadarObservation observation;
    float smoothed_rssi = -127;
    int64_t last_seen_ms = 0;
    std::array<RadarSample, 61> history {};
    uint8_t history_count = 0;
    RadarTrend trend = RadarTrend::Unknown;
};
struct RadarSnapshot {
    uint32_t session = 0, generation = 0;
    bool active = false, paused = false, limited = false, sound = false;
    RadarFilter filter = RadarFilter::All;
    uint32_t selected_id = 0;
    std::vector<RadarEmitter> emitters;
    std::string wifi_status = "Esperando Wi-Fi", ble_status = "Esperando BLE", error;
    uint32_t dropped_reports = 0;
};
struct RadarPoint { uint32_t id = 0; float x = 0, y = 0; RadarKind kind = RadarKind::Wifi; bool stale = false; };

std::string radar_address(const RadarObservation& observation);
const char* radar_trend_name(RadarTrend trend);
bool radar_matches(RadarKind kind, RadarFilter filter);
float radar_strength(float rssi);
std::vector<RadarPoint> radar_points(const RadarSnapshot& snapshot, int64_t now_ms);
std::vector<uint32_t> radar_hit_test(const std::vector<RadarPoint>& points, float x, float y, float radius);
std::string radar_ble_name(const uint8_t* bytes, size_t length);

// Portable model: no driver, LVGL, storage or wall-clock dependency.
class RadarModel {
public:
    static constexpr size_t kMaxEmitters = 128;
    uint32_t begin();
    void end();
    void pause(bool paused);
    bool observe(uint32_t session, const RadarObservation& observation, int64_t now_ms);
    void expire(int64_t now_ms);
    bool select(uint32_t id);
    void filter(RadarFilter filter);
    void sound(bool enabled);
    RadarSnapshot snapshot() const;
private:
    RadarSnapshot state_;
    uint32_t next_id_ = 1;
};
} // namespace el_sistema
