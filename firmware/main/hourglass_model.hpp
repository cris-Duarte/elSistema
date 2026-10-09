#pragma once

#include "home_scene.hpp"
#include <array>
#include <cstdint>

namespace el_sistema {

enum class HourglassState : uint8_t { Ready, Running, Paused, Completed };
enum class HourglassError : uint8_t { None, StandUpright, SensorUnavailable, Storage, Unavailable };

struct SandProfile {
    float gx = 0, gy = 1, threshold = 2, mound = 0;
    float score(float x, float y) const;
};

struct HourglassSnapshot {
    HourglassState state = HourglassState::Ready;
    HourglassState paused_from = HourglassState::Running;
    HourglassError error = HourglassError::None;
    uint16_t duration_minutes = 25;
    uint64_t elapsed_us = 0;
    double upper_mass = 1, lower_mass = 0;
    float gravity_x = 0, gravity_y = 1;
    SandProfile upper, lower;
    float stream_life = 0;
    int8_t flow_direction = 0, completed_direction = 1;
    bool sensor_available = false;
    uint32_t generation = 0;
    uint32_t frame = 0;
    uint32_t command_id = 0; // Ephemeral acknowledgement, not part of the NVS record.
};

// Shared geometry in glass coordinates: x is transverse, y is longitudinal;
// y=-1/+1 are the sealed ends and y=0 is the ONLY connection between chambers.
class HourglassScene {
public:
    static float half_width(float y);
    static bool contains(float x, float y);
    static SandProfile profile(double mass, float gx, float gy, bool receiving);
    static bool feeds_neck(const SandProfile& profile, int direction);
};

// Platform-independent engine. Time is supplied by the monotonic clock, never
// derived from frames or the RTC. Total mass is normalized to one.
class HourglassEngine {
public:
    bool start(uint16_t minutes, const MotionSample& motion, int64_t now_us);
    bool resume(const MotionSample& motion, int64_t now_us);
    void pause(int64_t now_us);
    void reset();
    void step(int64_t now_us, const MotionSample& motion);
    void restore(const HourglassSnapshot& checkpoint, int64_t now_us);
    const HourglassSnapshot& snapshot() const { return state_; }
    static bool valid_motion(const MotionSample& motion);
    static bool upright(const MotionSample& motion);

private:
    void profiles();
    void change(HourglassState state);
    HourglassSnapshot state_;
    int64_t last_us_ = 0;
    float settled_seconds_ = 0;
    int64_t reverse_since_us_ = 0;
};

inline constexpr size_t HOURGLASS_RECORD_BYTES = 48;
using HourglassRecord = std::array<uint8_t, HOURGLASS_RECORD_BYTES>;
HourglassRecord encode_hourglass(const HourglassSnapshot& snapshot);
bool decode_hourglass(const HourglassRecord& record, HourglassSnapshot& snapshot);
const char* hourglass_error_text(HourglassError error);

} // namespace el_sistema
