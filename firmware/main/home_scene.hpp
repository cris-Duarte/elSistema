#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace el_sistema {

// Tune the Home animation here. Acceleration is in g; distances are pixels.
inline constexpr size_t MAX_PARTICLES = 150;
inline constexpr float GRAVITY_SCALE = 720.0f;  // px/s² per g
inline constexpr float BOUNCE_DAMPING = 0.16f;
inline constexpr float SHAKE_THRESHOLD = 2.5f; // g (BMI270 configured ±8 g)
inline constexpr uint32_t HOME_FRAME_MS = 20;  // target 50 FPS
inline constexpr float PARTICLE_SPAWN_SECONDS = 0.09f; // ~11 grains/s, ±35% jitter

struct MotionSample {
    float accel_x = 0;
    float accel_y = 0;
    float accel_z = 0;
    float gyro_x = 0;  // degrees/second
    float gyro_y = 0;
    float gyro_z = 0;
    bool valid = false;
    bool gyro_available = false;
};

struct Particle {
    float x = 0;
    float y = 0;
    float vx = 0;
    float vy = 0;
    float radius = 5;
    uint32_t color = 0;
    bool active = false;
};
using SandParticle = Particle;

enum class ParticlePhase { Visible, Disappearing, Empty, Appearing };

class HomeScene {
public:
    static constexpr size_t kParticleCount = MAX_PARTICLES;
    explicit HomeScene(size_t count = MAX_PARTICLES);
    void reset(float width, float height, uint32_t seed = 0xA341316Cu);
    void resize(float width, float height, int quarter_turns);
    void step(float delta_seconds, const MotionSample& motion, int rotation);
    void perturb(float x, float y);

    const std::array<Particle, MAX_PARTICLES>& particles() const { return particles_; }
    size_t count() const { return count_; }
    size_t active_count() const { return active_count_; }
    float visibility() const { return visibility_; }
    ParticlePhase phase() const { return phase_; }
    uint32_t shake_count() const { return shake_count_; }
    float width() const { return width_; }
    float height() const { return height_; }
    static void orient_vector(float& x, float& y, int rotation);

private:
    void seed_particles();
    void emit_particles(float dt);
    float random_unit();
    void keep_in_bounds(Particle& particle);
    void collide();
    void update_disappearance(float dt, const MotionSample& motion);

    std::array<Particle, MAX_PARTICLES> particles_{};
    size_t count_;
    size_t active_count_ = 0;
    uint32_t random_state_ = 1;
    float spawn_seconds_ = 0;
    float next_spawn_seconds_ = 0;
    float width_ = 1;
    float height_ = 1;
    float visibility_ = 1;
    float phase_seconds_ = 0;
    float shake_cooldown_ = 0;
    float gravity_x_ = 0;
    float gravity_y_ = GRAVITY_SCALE;
    uint32_t shake_count_ = 0;
    ParticlePhase phase_ = ParticlePhase::Visible;
};

}  // namespace el_sistema
