#include "radar_model.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace el_sistema {
namespace {
bool same(const RadarObservation& a, const RadarObservation& b)
{
    return a.kind == b.kind && a.address == b.address &&
        (a.kind == RadarKind::Wifi || a.address_type == b.address_type);
}
RadarTrend trend(const RadarEmitter& e, int64_t now)
{
    float old = 0, recent = 0; int old_count = 0, recent_count = 0;
    for (size_t i = 0; i < e.history_count; ++i) {
        const auto& s = e.history[i];
        const int64_t age = now - s.time_ms;
        if (age <= 5000) { recent += s.rssi; ++recent_count; }
        else if (age <= 15000) { old += s.rssi; ++old_count; }
    }
    if (!old_count || !recent_count) return RadarTrend::Unknown;
    const float delta = recent / recent_count - old / old_count;
    return delta >= 5 ? RadarTrend::Increasing : delta <= -5 ? RadarTrend::Decreasing : RadarTrend::Stable;
}
}
std::string radar_address(const RadarObservation& o)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", o.address[0], o.address[1],
                  o.address[2], o.address[3], o.address[4], o.address[5]);
    return text;
}
const char* radar_trend_name(RadarTrend t)
{
    switch (t) {
        case RadarTrend::Increasing: return "Aumentando";
        case RadarTrend::Decreasing: return "Disminuyendo";
        case RadarTrend::Stable: return "Estable";
        default: return "Midiendo tendencia";
    }
}
bool radar_matches(RadarKind k, RadarFilter f)
{ return f == RadarFilter::All || (f == RadarFilter::Wifi && k == RadarKind::Wifi) || (f == RadarFilter::Ble && k == RadarKind::Ble); }
float radar_strength(float rssi) { return std::clamp((rssi + 100.f) / 65.f, 0.f, 1.f); }
std::vector<RadarPoint> radar_points(const RadarSnapshot& s, int64_t now)
{
    std::vector<RadarPoint> result;
    for (const auto& e : s.emitters) {
        if (!radar_matches(e.observation.kind, s.filter)) continue;
        if (result.size() == 24 && e.id != s.selected_id) continue;
        if (result.size() == 24) result.pop_back();
        uint32_t hash = 2166136261u;
        for (uint8_t byte : e.observation.address) hash = (hash ^ byte) * 16777619u;
        hash = (hash ^ e.observation.address_type) * 16777619u;
        hash = (hash ^ static_cast<uint8_t>(e.observation.kind)) * 16777619u;
        const float angle = static_cast<float>(hash % 36000) * 0.0001745329252f;
        const float radius = 0.15f + 0.72f * (1.f - radar_strength(e.smoothed_rssi));
        result.push_back({e.id, std::cos(angle) * radius, std::sin(angle) * radius,
                          e.observation.kind, now - e.last_seen_ms >= 15000});
    }
    return result;
}
std::vector<uint32_t> radar_hit_test(const std::vector<RadarPoint>& points, float x, float y, float radius)
{
    std::vector<uint32_t> hits;
    for (const auto& p : points)
        if ((p.x-x)*(p.x-x) + (p.y-y)*(p.y-y) <= radius*radius) hits.push_back(p.id);
    return hits;
}
std::string radar_ble_name(const uint8_t* bytes, size_t length)
{
    std::string short_name;
    for (size_t i = 0; i < length;) {
        const size_t field = bytes[i++];
        if (!field || field > length - i) break;
        const uint8_t type = bytes[i];
        if ((type == 8 || type == 9) && field > 1) {
            std::string name(reinterpret_cast<const char*>(bytes+i+1), field-1);
            for (char& ch : name) if (static_cast<unsigned char>(ch) < 32) ch = ' ';
            if (type == 9) return name;
            short_name = name;
        }
        i += field;
    }
    return short_name;
}
uint32_t RadarModel::begin()
{
    state_.active = true; state_.paused = false;
    ++state_.session; if (!state_.session) ++state_.session;
    ++state_.generation;
    return state_.session;
}
void RadarModel::end() { state_.active = false; ++state_.session; ++state_.generation; }
void RadarModel::pause(bool p) { state_.paused = p; ++state_.session; ++state_.generation; }
bool RadarModel::observe(uint32_t session, const RadarObservation& o, int64_t now)
{
    if (!state_.active || state_.paused || session != state_.session || o.rssi == 127 || o.rssi < -127 || o.rssi > 20) return false;
    auto it = std::find_if(state_.emitters.begin(), state_.emitters.end(), [&](const RadarEmitter& e) { return same(e.observation, o); });
    if (it == state_.emitters.end()) {
        if (state_.emitters.size() >= kMaxEmitters) {
            it = std::min_element(state_.emitters.begin(), state_.emitters.end(), [&](const RadarEmitter& a, const RadarEmitter& b) {
                if (a.id == state_.selected_id) return false;
                if (b.id == state_.selected_id) return true;
                return a.last_seen_ms < b.last_seen_ms;
            });
            state_.emitters.erase(it); state_.limited = true;
        }
        RadarEmitter e; e.id = next_id_++; e.observation = o; e.smoothed_rssi = o.rssi;
        state_.emitters.push_back(e); it = state_.emitters.end()-1;
        if (state_.emitters.size() == kMaxEmitters) state_.limited = true;
    } else {
        if (now - it->last_seen_ms >= 15000) { it->history_count = 0; it->smoothed_rssi = o.rssi; }
        else it->smoothed_rssi += 0.25f * (o.rssi - it->smoothed_rssi);
        const std::string previous_name = it->observation.name;
        it->observation = o;
        if (o.name.empty()) it->observation.name = previous_name;
    }
    if (it->observation.name.empty()) it->observation.name = o.kind == RadarKind::Wifi ? "Red oculta" : "BLE sin nombre";
    it->last_seen_ms = now;
    size_t removed = 0;
    while (removed < it->history_count && now - it->history[removed].time_ms > 60000) ++removed;
    if (removed) {
        std::move(it->history.begin()+removed, it->history.begin()+it->history_count, it->history.begin());
        it->history_count -= removed;
    }
    if (it->history_count && now/1000 == it->history[it->history_count-1].time_ms/1000)
        it->history[it->history_count-1] = {now, it->smoothed_rssi};
    else {
        if (it->history_count == it->history.size()) {
            std::move(it->history.begin()+1, it->history.end(), it->history.begin()); --it->history_count;
        }
        it->history[it->history_count++] = {now, it->smoothed_rssi};
    }
    it->trend = trend(*it, now); ++state_.generation;
    return true;
}
void RadarModel::expire(int64_t now)
{
    const size_t count = state_.emitters.size();
    state_.emitters.erase(std::remove_if(state_.emitters.begin(), state_.emitters.end(), [&](const RadarEmitter& e) {
        return e.id != state_.selected_id && now-e.last_seen_ms >= 60000;
    }), state_.emitters.end());
    if (count != state_.emitters.size()) ++state_.generation;
}
bool RadarModel::select(uint32_t id)
{
    if (id && std::none_of(state_.emitters.begin(), state_.emitters.end(), [&](const RadarEmitter& e) { return e.id == id; })) return false;
    state_.selected_id = id; ++state_.generation; return true;
}
void RadarModel::filter(RadarFilter f) { state_.filter = f; ++state_.generation; }
void RadarModel::sound(bool on) { state_.sound = on; ++state_.generation; }
RadarSnapshot RadarModel::snapshot() const
{
    RadarSnapshot copy = state_;
    std::stable_sort(copy.emitters.begin(), copy.emitters.end(), [](const RadarEmitter& a, const RadarEmitter& b) { return a.smoothed_rssi > b.smoothed_rssi; });
    return copy;
}
} // namespace el_sistema
