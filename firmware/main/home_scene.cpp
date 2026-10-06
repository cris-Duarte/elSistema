#include "home_scene.hpp"
#include "theme.hpp"

#include <algorithm>
#include <cmath>

namespace el_sistema {
namespace {
constexpr float kMaxStep = 0.05f;
constexpr float kFadeSeconds = 0.45f;
constexpr float kEmptySeconds = 0.18f;
constexpr float kAppearSeconds = 0.45f;
constexpr float kFriction = 0.82f;
constexpr uint32_t kColors[] = {theme::HomeParticleCyan, theme::HomeParticleLime,
                               theme::HomeParticleRose, theme::HomeParticleLavender};
}

HomeScene::HomeScene(size_t count) : count_(std::clamp(count, size_t{1}, MAX_PARTICLES)) {}

void HomeScene::orient_vector(float& x, float& y, int rotation)
{
    const float old_x = x, old_y = y;
    // Same physical axes as OrientationService: +Y=0°, +X=90°.
    switch (rotation & 3) {
        case 1: x = -old_y; y = old_x; break;
        case 2: x = -old_x; y = -old_y; break;
        case 3: x = old_y; y = -old_x; break;
        default: break;
    }
}

void HomeScene::reset(float width, float height, uint32_t seed)
{
    width_ = std::max(32.0f, width);
    height_ = std::max(32.0f, height);
    visibility_ = 1;
    phase_ = ParticlePhase::Visible;
    phase_seconds_ = shake_cooldown_ = 0;
    shake_count_ = 0;
    gravity_x_ = 0;
    gravity_y_ = GRAVITY_SCALE;
    random_state_ = seed == 0 ? 1 : seed;
    seed_particles();
}

float HomeScene::random_unit()
{
    // Local PRNG keeps physics independent of the platform and repeatable in tests.
    random_state_ ^= random_state_ << 13;
    random_state_ ^= random_state_ >> 17;
    random_state_ ^= random_state_ << 5;
    return static_cast<float>(random_state_ >> 8) / 16777216.0f;
}

void HomeScene::seed_particles()
{
    active_count_ = 0;
    spawn_seconds_ = next_spawn_seconds_ = 0;
    for (size_t index = 0; index < count_; ++index) {
        Particle& particle = particles_[index];
        particle = {};
        particle.radius = 4.5f + random_unit() * 2.1f;
        particle.color = kColors[index % 4];
    }
}

void HomeScene::emit_particles(float dt)
{
    spawn_seconds_ += dt;
    while (active_count_ < count_ && spawn_seconds_ >= next_spawn_seconds_) {
        spawn_seconds_ -= next_spawn_seconds_;
        Particle& particle = particles_[active_count_++];
        particle.active = true;
        particle.x = particle.radius + random_unit() * (width_ - 2 * particle.radius);
        particle.y = particle.radius; // Enter at the top, never inside the viewport.
        particle.vx = (random_unit() - 0.5f) * 30.0f;
        particle.vy = 35.0f + random_unit() * 30.0f;
        next_spawn_seconds_ = PARTICLE_SPAWN_SECONDS * (0.65f + random_unit() * 0.7f);
    }
}

void HomeScene::resize(float width, float height, int quarter_turns)
{
    const float old_width = width_, old_height = height_;
    width_ = std::max(32.0f, width);
    height_ = std::max(32.0f, height);
    const int turns = (quarter_turns % 4 + 4) % 4;
    for (size_t index = 0; index < active_count_; ++index) {
        Particle& p = particles_[index];
        const float x = p.x, y = p.y;
        switch (turns) {
            case 1: p.x = old_height - y; p.y = x; break;
            case 2: p.x = old_width - x; p.y = old_height - y; break;
            case 3: p.x = y; p.y = old_width - x; break;
            default: p.x *= width_ / old_width; p.y *= height_ / old_height; break;
        }
        // Position and screen velocity rotate together, not the physical gravity.
        orient_vector(p.vx, p.vy, turns);
        keep_in_bounds(p);
    }
    orient_vector(gravity_x_, gravity_y_, turns);
}

void HomeScene::update_disappearance(float dt, const MotionSample& motion)
{
    shake_cooldown_ = std::max(0.0f, shake_cooldown_ - dt);
    const float magnitude_squared = motion.accel_x * motion.accel_x + motion.accel_y * motion.accel_y +
                                    motion.accel_z * motion.accel_z;
    if (motion.valid && phase_ == ParticlePhase::Visible && shake_cooldown_ == 0 &&
        magnitude_squared > SHAKE_THRESHOLD * SHAKE_THRESHOLD) {
        phase_ = ParticlePhase::Disappearing;
        phase_seconds_ = 0;
        shake_cooldown_ = 2.0f;
        ++shake_count_;
    }
    phase_seconds_ += dt;
    switch (phase_) {
        case ParticlePhase::Visible: visibility_ = 1; break;
        case ParticlePhase::Disappearing:
            visibility_ = std::max(0.0f, 1.0f - phase_seconds_ / kFadeSeconds);
            if (phase_seconds_ >= kFadeSeconds) { phase_ = ParticlePhase::Empty; phase_seconds_ = 0; }
            break;
        case ParticlePhase::Empty:
            visibility_ = 0;
            if (phase_seconds_ >= kEmptySeconds) {
                seed_particles();
                phase_ = ParticlePhase::Appearing;
                phase_seconds_ = 0;
            }
            break;
        case ParticlePhase::Appearing:
            visibility_ = std::min(1.0f, phase_seconds_ / kAppearSeconds);
            if (phase_seconds_ >= kAppearSeconds) { phase_ = ParticlePhase::Visible; phase_seconds_ = 0; }
            break;
    }
}

void HomeScene::step(float delta_seconds, const MotionSample& motion, int rotation)
{
    const float dt = std::clamp(delta_seconds, 0.0f, kMaxStep);
    if (!(dt > 0)) return;
    MotionSample safe = motion;
    safe.valid = safe.valid && std::isfinite(safe.accel_x) && std::isfinite(safe.accel_y) && std::isfinite(safe.accel_z);
    update_disappearance(dt, safe);
    if (phase_ == ParticlePhase::Empty) return;
    if (phase_ == ParticlePhase::Visible || phase_ == ParticlePhase::Appearing) emit_particles(dt);

    float ax = 0, ay = 1;
    if (safe.valid) {
        ax = safe.accel_x;
        ay = safe.accel_y;
        orient_vector(ax, ay, rotation);
        // Lying flat has almost no in-plane gravity: keep a gentle downwards
        // pull instead of freezing the scene. Other inclinations use both axes.
        if (std::abs(ax) + std::abs(ay) < 0.08f) { ax = 0; ay = 0.22f; }
    }
    const float smoothing = std::min(1.0f, dt * 12.0f);
    gravity_x_ += (std::clamp(ax, -1.5f, 1.5f) * GRAVITY_SCALE - gravity_x_) * smoothing;
    gravity_y_ += (std::clamp(ay, -1.5f, 1.5f) * GRAVITY_SCALE - gravity_y_) * smoothing;
    float angular = 0;
    if (safe.gyro_available && std::isfinite(safe.gyro_z)) angular = std::clamp(safe.gyro_z * 0.3f, -40.0f, 40.0f);

    // Two short substeps avoid tunnelling, three bounded contact passes allow
    // stable piles. At 150 particles this is <70k cheap distance tests/frame.
    const float sub_dt = dt * 0.5f;
    const float drag = std::exp(-1.2f * sub_dt);
    for (int substep = 0; substep < 2; ++substep) {
        for (size_t index = 0; index < active_count_; ++index) {
            Particle& p = particles_[index];
            p.vx = std::clamp((p.vx + (gravity_x_ + angular) * sub_dt) * drag, -650.0f, 650.0f);
            p.vy = std::clamp((p.vy + gravity_y_ * sub_dt) * drag, -650.0f, 650.0f);
            p.x += p.vx * sub_dt;
            p.y += p.vy * sub_dt;
            keep_in_bounds(p);
        }
        for (int pass = 0; pass < 3; ++pass) collide();
    }
}

void HomeScene::collide()
{
    for (size_t left = 0; left < active_count_; ++left) {
        for (size_t right = left + 1; right < active_count_; ++right) {
            Particle& a = particles_[left];
            Particle& b = particles_[right];
            float dx = b.x - a.x, dy = b.y - a.y;
            const float minimum = a.radius + b.radius;
            if (std::abs(dx) >= minimum || std::abs(dy) >= minimum) continue;
            float squared = dx * dx + dy * dy;
            if (squared >= minimum * minimum) continue;
            if (squared < 0.0001f) { dx = 0.01f; dy = 0; squared = 0.0001f; }
            const float distance = std::sqrt(squared);
            const float nx = dx / distance, ny = dy / distance;
            const float overlap = (minimum - distance) * 0.5f;
            a.x -= nx * overlap; a.y -= ny * overlap;
            b.x += nx * overlap; b.y += ny * overlap;
            const float relative = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
            if (relative < 0) {
                const float impulse = -(1.0f + BOUNCE_DAMPING * 0.5f) * relative * 0.5f;
                a.vx -= impulse * nx; a.vy -= impulse * ny;
                b.vx += impulse * nx; b.vy += impulse * ny;
                const float tangent = ((b.vx - a.vx) * -ny + (b.vy - a.vy) * nx) * 0.12f;
                a.vx -= ny * tangent; a.vy += nx * tangent;
                b.vx += ny * tangent; b.vy -= nx * tangent;
            }
            // Inelastic grain contacts dissipate energy even when a stack's
            // position solver sees an overlap without an approaching velocity.
            a.vx *= 0.92f; a.vy *= 0.92f;
            b.vx *= 0.92f; b.vy *= 0.92f;
            keep_in_bounds(a);
            keep_in_bounds(b);
        }
    }
}

void HomeScene::perturb(float x, float y)
{
    if (visibility_ == 0) return;
    for (size_t index = 0; index < active_count_; ++index) {
        Particle& p = particles_[index];
        const float dx = p.x - x, dy = p.y - y;
        const float squared = dx * dx + dy * dy;
        constexpr float radius = 140.0f;
        if (squared >= radius * radius) continue;
        const float distance = std::max(1.0f, std::sqrt(squared));
        const float strength = (1 - distance / radius) * 310.0f;
        p.vx += distance <= 1 ? strength * 0.5f : dx / distance * strength;
        p.vy += dy / distance * strength - 100.0f;
    }
}

void HomeScene::keep_in_bounds(Particle& p)
{
    const float max_x = std::max(p.radius, width_ - p.radius);
    const float max_y = std::max(p.radius, height_ - p.radius);
    if (p.x < p.radius || p.x > max_x) {
        const bool left = p.x < p.radius;
        p.x = left ? p.radius : max_x;
        p.vx = (left ? 1 : -1) * std::abs(p.vx) * BOUNCE_DAMPING;
        p.vy *= kFriction;
        if (std::abs(p.vx) < 12) p.vx = 0;
    }
    if (p.y < p.radius || p.y > max_y) {
        const bool top = p.y < p.radius;
        p.y = top ? p.radius : max_y;
        p.vy = (top ? 1 : -1) * std::abs(p.vy) * BOUNCE_DAMPING;
        p.vx *= kFriction;
        if (std::abs(p.vy) < 12) p.vy = 0;
    }
}

}  // namespace el_sistema
