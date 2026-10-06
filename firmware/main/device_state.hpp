#pragma once

#include "home_scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace el_sistema {

// INA226 register 02h is a positive 15-bit value with a 1.25 mV LSB,
// unlike INA219: no three-bit right shift.
inline int ina226_battery_mv(uint16_t raw)
{
    return (raw & 0x8000) ? -1 : static_cast<int>(static_cast<uint32_t>(raw) * 5 / 4);
}

inline int battery_percent_from_mv(int pack_mv)
{
    if (pack_mv < 5000 || pack_mv > 9000) return -1;
    // Same approximate 2S voltage-based scale as M5Unified, not a fuel gauge.
    return std::clamp((pack_mv / 2 - 3300) * 100 / 800, 0, 100);
}

class OrientationDetector {
public:
    explicit OrientationDetector(int initial = 1) : rotation_(initial & 3) {}

    int update(const MotionSample& sample, int64_t now_us)
    {
        int next = -1;
        if (sample.valid && std::isfinite(sample.accel_x) && std::isfinite(sample.accel_y) &&
            std::isfinite(sample.accel_z)) {
            const float x = std::abs(sample.accel_x), y = std::abs(sample.accel_y);
            const float magnitude = x * x + y * y + sample.accel_z * sample.accel_z;
            // Do not rotate on a shake, near-zero readings or a nearly flat
            // device. A dead band keeps the applied axis near the diagonal.
            if (std::max(x, y) >= 0.28f && magnitude >= 0.25f && magnitude <= 2.56f) {
                const bool x_axis = x > y + 0.08f ? true : y > x + 0.08f ? false : (rotation_ & 1);
                next = x_axis ? (sample.accel_x < 0 ? 3 : 1) : (sample.accel_y > 0 ? 0 : 2);
            }
        }
        if (next < 0 || next == rotation_) {
            candidate_ = -1;
        } else if (next != candidate_ || now_us < candidate_since_us_) {
            candidate_ = next;
            candidate_since_us_ = now_us;
        } else if (now_us - candidate_since_us_ >= 800000) {
            rotation_ = next;
            candidate_ = -1;
        }
        return rotation_;
    }

private:
    int rotation_;
    int candidate_ = -1;
    int64_t candidate_since_us_ = 0;
};

inline int shortest_rotation_turns(int from, int to)
{
    const int turns = (to - from + 4) & 3;
    return turns == 3 ? -1 : turns;
}

struct ScreenPoint { float x = 0, y = 0; };
inline ScreenPoint rotate_screen_point(ScreenPoint point, int width, int height, int turns)
{
    switch ((turns % 4 + 4) % 4) {
        case 1: return {height - point.y, point.x};
        case 2: return {width - point.x, height - point.y};
        case 3: return {point.y, width - point.x};
        default: return point;
    }
}

} // namespace el_sistema
