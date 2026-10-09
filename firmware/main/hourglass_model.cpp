#include "hourglass_model.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace el_sistema {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kSandCapacity = 0.58f;
constexpr float kStreamSeconds = 0.45f;
uint32_t checksum(const uint8_t* bytes, size_t count)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
void put(uint8_t* target, uint64_t value, size_t count)
{
    for (size_t i = 0; i < count; ++i) target[i] = static_cast<uint8_t>(value >> (i * 8));
}
uint64_t get(const uint8_t* source, size_t count)
{
    uint64_t value = 0;
    for (size_t i = 0; i < count; ++i) value |= uint64_t(source[i]) << (i * 8);
    return value;
}
void put_float(uint8_t* target, float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    put(target, bits, 4);
}
float get_float(const uint8_t* source)
{
    const uint32_t bits = static_cast<uint32_t>(get(source, 4));
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
}

float SandProfile::score(float x, float y) const
{
    // A receiving pile rises under the stream with an angle of repose; a
    // draining bowl has a shallow depression towards its outlet.
    return gx * x + gy * y - mound * std::abs(gy * x - gx * y);
}

float HourglassScene::half_width(float y)
{
    const float u = std::clamp(std::abs(y), 0.0f, 1.0f);
    return 0.035f + 0.60f * std::sin(kPi * 0.5f * std::pow(u, 0.8f)) - 0.12f * std::pow(u, 6.0f);
}

bool HourglassScene::contains(float x, float y)
{
    return std::isfinite(x) && std::isfinite(y) && std::abs(y) <= 1 && std::abs(x) <= half_width(y);
}

SandProfile HourglassScene::profile(double mass, float gx, float gy, bool receiving)
{
    const float length = std::hypot(gx, gy);
    SandProfile result;
    if (length > 0.02f) { result.gx = gx / length; result.gy = gy / length; }
    result.mound = receiving ? 0.55f * std::abs(result.gy) : -0.10f * std::abs(result.gy);
    if (mass <= 0) { result.threshold = 3; return result; }
    // Quantile of gravity potential inside the curved chamber conserves visual
    // occupied area across inclinations. Histogram avoids sorting/all-pairs.
    std::array<unsigned, 128> bins{};
    unsigned total = 0;
    for (int row = 0; row < 64; ++row) {
        const float y = (row + 0.5f) / 64.0f;
        const float width = half_width(y);
        for (int col = 0; col < 48; ++col) {
            const float x = -0.65f + (col + 0.5f) * 1.3f / 48.0f;
            if (std::abs(x) > width) continue;
            const int bin = std::clamp(static_cast<int>((result.score(x, y) + 2) * 32), 0, 127);
            ++bins[bin]; ++total;
        }
    }
    const float target = std::clamp(static_cast<float>(mass), 0.0f, 1.0f) * kSandCapacity * total;
    unsigned filled = 0;
    for (int bin = 127; bin >= 0; --bin) {
        if (filled + bins[bin] >= target && bins[bin]) {
            result.threshold = (bin + 1.0f - (target - filled) / bins[bin]) / 32.0f - 2;
            break;
        }
        filled += bins[bin];
    }
    return result;
}

bool HourglassScene::feeds_neck(const SandProfile& profile, int direction)
{
    // Profiles use each chamber's positive local y. The supplying chamber has
    // gravity towards its neck (negative local y).
    (void)direction;
    return profile.score(0, 0) >= profile.threshold;
}

bool HourglassEngine::valid_motion(const MotionSample& motion)
{
    return motion.valid && std::isfinite(motion.accel_x) && std::isfinite(motion.accel_y) &&
           std::isfinite(motion.accel_z);
}

bool HourglassEngine::upright(const MotionSample& motion)
{
    if (!valid_motion(motion)) return false;
    const float magnitude = std::sqrt(motion.accel_x * motion.accel_x + motion.accel_y * motion.accel_y +
                                      motion.accel_z * motion.accel_z);
    return magnitude >= 0.65f && magnitude <= 1.35f && std::abs(motion.accel_y) >= 0.75f &&
           std::abs(motion.accel_x) <= 0.45f;
}

void HourglassEngine::change(HourglassState state)
{
    state_.state = state;
    ++state_.generation;
}

void HourglassEngine::profiles()
{
    float gx = state_.gravity_x, gy = state_.gravity_y;
    if (std::hypot(gx, gy) < 0.08f) {
        // Lying flat removes in-plane gravity; keep the last surface direction
        // rather than making the pile fall down under a fabricated force.
        gx = state_.lower.gx;
        gy = state_.lower.gy;
    }
    // Mirror local y for the upper bowl. Both profiles share physical x.
    state_.upper = HourglassScene::profile(state_.upper_mass, gx, -gy, gy < 0);
    state_.lower = HourglassScene::profile(state_.lower_mass, gx, gy, gy >= 0);
}

bool HourglassEngine::start(uint16_t minutes, const MotionSample& motion, int64_t now_us)
{
    if (minutes < 1 || minutes > 120) return false;
    if (!upright(motion)) {
        state_.error = valid_motion(motion) ? HourglassError::StandUpright : HourglassError::SensorUnavailable;
        ++state_.generation;
        return false;
    }
    state_.duration_minutes = minutes;
    state_.upper_mass = motion.accel_y > 0 ? 1 : 0;
    state_.lower_mass = 1 - state_.upper_mass;
    state_.gravity_x = motion.accel_x;
    state_.gravity_y = motion.accel_y;
    state_.elapsed_us = 0;
    state_.stream_life = 0;
    state_.flow_direction = motion.accel_y > 0 ? 1 : -1;
    state_.sensor_available = true;
    state_.error = HourglassError::None;
    settled_seconds_ = 0;
    reverse_since_us_ = 0;
    last_us_ = now_us;
    profiles();
    change(HourglassState::Running);
    return true;
}

void HourglassEngine::pause(int64_t now_us)
{
    if (state_.state != HourglassState::Running && state_.state != HourglassState::Completed) return;
    if (state_.state == HourglassState::Running && now_us > last_us_) state_.elapsed_us += now_us - last_us_;
    state_.paused_from = state_.state;
    reverse_since_us_ = 0;
    last_us_ = now_us;
    change(HourglassState::Paused);
}

bool HourglassEngine::resume(const MotionSample& motion, int64_t now_us)
{
    if (state_.state != HourglassState::Paused) return false;
    if (!valid_motion(motion)) {
        state_.error = HourglassError::SensorUnavailable;
        ++state_.generation;
        return false;
    }
    state_.error = HourglassError::None;
    last_us_ = now_us;
    change(state_.paused_from);
    return true;
}

void HourglassEngine::reset()
{
    const uint16_t duration = state_.duration_minutes;
    const uint32_t generation = state_.generation + 1;
    state_ = {};
    state_.duration_minutes = duration;
    state_.generation = generation;
    settled_seconds_ = 0;
    reverse_since_us_ = 0;
    profiles();
}

void HourglassEngine::step(int64_t now_us, const MotionSample& motion)
{
    const bool valid = valid_motion(motion);
    if (valid != state_.sensor_available) { state_.sensor_available = valid; ++state_.generation; }
    if (!valid) {
        if (state_.state == HourglassState::Running || state_.state == HourglassState::Completed) {
            // Do not attribute a stale interval to physical flow or lost samples.
            pause(last_us_);
            state_.error = HourglassError::SensorUnavailable;
            ++state_.generation;
        }
        last_us_ = now_us;
        return;
    }
    if (state_.state == HourglassState::Paused || state_.state == HourglassState::Ready) {
        last_us_ = now_us;
        return;
    }
    const int64_t dt_us = std::max<int64_t>(0, now_us - last_us_);
    const float dt = dt_us / 1000000.0f;
    last_us_ = now_us;
    const float smoothing = 1 - std::exp(-std::min(dt, 1.0f) * 12);
    state_.gravity_x += (std::clamp(motion.accel_x, -1.5f, 1.5f) - state_.gravity_x) * smoothing;
    state_.gravity_y += (std::clamp(motion.accel_y, -1.5f, 1.5f) - state_.gravity_y) * smoothing;
    profiles();
    ++state_.frame;
    const int direction = state_.gravity_y > 0.10f ? 1 : state_.gravity_y < -0.10f ? -1 : 0;

    if (state_.state == HourglassState::Completed) {
        if (upright(motion) && motion.accel_y * state_.completed_direction < -0.75f &&
            direction == -state_.completed_direction &&
            std::abs(state_.gravity_y) >= 0.70f) {
            if (reverse_since_us_ == 0) reverse_since_us_ = now_us;
            if (now_us - reverse_since_us_ >= 800000) {
                const SandProfile& source = direction > 0 ? state_.upper : state_.lower;
                if (HourglassScene::feeds_neck(source, direction)) {
                    state_.elapsed_us = 0;
                    state_.stream_life = 0;
                    settled_seconds_ = 0;
                    state_.flow_direction = direction;
                    reverse_since_us_ = 0;
                    change(HourglassState::Running);
                }
            }
        } else reverse_since_us_ = 0;
        return;
    }

    state_.elapsed_us += dt_us;
    state_.stream_life = std::max(0.0f, state_.stream_life - dt);
    const SandProfile& source = direction > 0 ? state_.upper : state_.lower;
    const double available = direction > 0 ? state_.upper_mass : state_.lower_mass;
    const float factor = std::clamp((std::abs(state_.gravity_y) - 0.10f) / 0.90f, 0.0f, 1.0f);
    if (direction && available > 0 && HourglassScene::feeds_neck(source, direction)) {
        const double amount = std::min(available, dt_us / (double(state_.duration_minutes) * 60000000) * factor);
        state_.upper_mass = std::clamp(state_.upper_mass - direction * amount, 0.0, 1.0);
        state_.lower_mass = 1 - state_.upper_mass;
        if (amount > 0) {
            state_.flow_direction = direction;
            state_.stream_life = kStreamSeconds;
            settled_seconds_ = 0;
        }
    }
    const double remaining = direction > 0 ? state_.upper_mass : state_.lower_mass;
    if (direction && remaining < 1e-9 && state_.stream_life <= 0 && upright(motion)) {
        settled_seconds_ += dt;
        if (settled_seconds_ >= 0.25f) {
            state_.completed_direction = direction;
            reverse_since_us_ = 0;
            change(HourglassState::Completed);
        }
    } else settled_seconds_ = 0;
    profiles();
}

void HourglassEngine::restore(const HourglassSnapshot& checkpoint, int64_t now_us)
{
    state_ = checkpoint;
    if (state_.state == HourglassState::Running || state_.state == HourglassState::Completed) {
        state_.paused_from = state_.state;
        state_.state = HourglassState::Paused;
    }
    state_.sensor_available = false;
    state_.error = HourglassError::None;
    state_.stream_life = 0;
    last_us_ = now_us;
    settled_seconds_ = 0;
    reverse_since_us_ = 0;
    profiles();
    ++state_.generation;
}

HourglassRecord encode_hourglass(const HourglassSnapshot& state)
{
    HourglassRecord record{};
    record[0] = 'H'; record[1] = 'G'; record[2] = 1;
    put(record.data() + 4, state.duration_minutes, 2);
    record[6] = static_cast<uint8_t>(state.state);
    record[7] = static_cast<uint8_t>(state.paused_from);
    put(record.data() + 8, state.elapsed_us, 8);
    put_float(record.data() + 16, static_cast<float>(state.upper_mass));
    put_float(record.data() + 20, state.gravity_x);
    put_float(record.data() + 24, state.gravity_y);
    record[28] = state.completed_direction > 0 ? 1 : 2;
    put_float(record.data() + 32, state.lower.gx);
    put_float(record.data() + 36, state.lower.gy);
    put(record.data() + 44, checksum(record.data(), 44), 4);
    return record;
}

bool decode_hourglass(const HourglassRecord& record, HourglassSnapshot& state)
{
    if (record[0] != 'H' || record[1] != 'G' || record[2] != 1 ||
        get(record.data() + 44, 4) != checksum(record.data(), 44)) return false;
    const uint16_t duration = static_cast<uint16_t>(get(record.data() + 4, 2));
    const float mass = get_float(record.data() + 16);
    const float gx = get_float(record.data() + 20), gy = get_float(record.data() + 24);
    const float surface_x = get_float(record.data() + 32), surface_y = get_float(record.data() + 36);
    if (duration < 1 || duration > 120 || record[6] > 3 ||
        (record[7] != 1 && record[7] != 3) || !std::isfinite(mass) || mass < 0 || mass > 1 ||
        !std::isfinite(gx) || !std::isfinite(gy) || std::abs(gx) > 1.5f || std::abs(gy) > 1.5f ||
        !std::isfinite(surface_x) || !std::isfinite(surface_y) || std::abs(surface_x) > 1.01f ||
        std::abs(surface_y) > 1.01f || (record[28] != 1 && record[28] != 2)) return false;
    state = {};
    state.duration_minutes = duration;
    state.state = static_cast<HourglassState>(record[6]);
    state.paused_from = static_cast<HourglassState>(record[7]);
    state.elapsed_us = get(record.data() + 8, 8);
    state.upper_mass = mass;
    state.lower_mass = 1 - state.upper_mass;
    state.gravity_x = gx; state.gravity_y = gy;
    state.lower.gx = surface_x; state.lower.gy = surface_y;
    state.completed_direction = record[28] == 1 ? 1 : -1;
    return true;
}

const char* hourglass_error_text(HourglassError error)
{
    switch (error) {
        case HourglassError::None: return "";
        case HourglassError::StandUpright: return "Coloca la Tab5 vertical para iniciar el reloj.";
        case HourglassError::SensorUnavailable: return "Movimiento no disponible. Reanuda cuando se recupere el sensor.";
        case HourglassError::Storage: return "No se pudo guardar la sesión. Se volverá a intentar.";
        case HourglassError::Unavailable: return "El reloj no pudo iniciar. Vuelve a Inicio e inténtalo de nuevo.";
    }
    return "";
}
} // namespace el_sistema
