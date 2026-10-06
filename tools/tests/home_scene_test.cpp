#include "home_scene.hpp"
#include "theme.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace el_sistema;

static float mean_x(const HomeScene& scene)
{
    float sum = 0;
    for (size_t i = 0; i < scene.active_count(); ++i) sum += scene.particles()[i].x;
    return scene.active_count() ? sum / scene.active_count() : 0;
}
static float mean_y(const HomeScene& scene)
{
    float sum = 0;
    for (size_t i = 0; i < scene.active_count(); ++i) sum += scene.particles()[i].y;
    return scene.active_count() ? sum / scene.active_count() : 0;
}
static void bounds(const HomeScene& scene)
{
    for (size_t i = 0; i < scene.count(); ++i) {
        const Particle& p = scene.particles()[i];
        assert(p.color != 0);
        assert(p.active == (i < scene.active_count()));
        if (!p.active) continue;
        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.vx) && std::isfinite(p.vy));
        assert(p.x >= p.radius - .01f && p.x <= scene.width() - p.radius + .01f);
        assert(p.y >= p.radius - .01f && p.y <= scene.height() - p.radius + .01f);
    }
}

int main()
{
    static_assert(theme::HomeBackground == 0x000000);
    static_assert(theme::HomeInk != theme::HomeIconInk);
    assert(HomeScene{}.count() == 150);
    assert(HomeScene{999}.count() == MAX_PARTICLES);
    assert(HomeScene{0}.count() == 1);
    HomeScene emitter;
    emitter.reset(1280, 720);
    const uint32_t light_colors[] = {theme::HomeParticleCyan, theme::HomeParticleLime,
                                   theme::HomeParticleRose, theme::HomeParticleLavender};
    for (size_t i = 0; i < emitter.count(); ++i) {
        assert(emitter.particles()[i].color == light_colors[i % 4]);
    }
    assert(emitter.active_count() == 0);
    float first_x = 0;
    bool varied_x = false;
    for (int frame = 0; frame < 1000; ++frame) {
        const size_t before = emitter.active_count();
        emitter.step(.02f, {}, 0);
        assert(emitter.active_count() <= before + 1); // Not one giant first-frame spawn.
        if (emitter.active_count() > before) {
            const Particle& born = emitter.particles()[before];
            assert(born.y >= born.radius && born.y < born.radius + 8);
            if (before == 0) first_x = born.x;
            else varied_x |= std::abs(first_x - born.x) > 20;
        }
        if (frame == 0) assert(emitter.active_count() == 1);
        if (frame == 49) assert(emitter.active_count() >= 8 && emitter.active_count() <= 18);
        bounds(emitter);
    }
    assert(emitter.active_count() == MAX_PARTICLES && varied_x);
    HomeScene another_seed;
    another_seed.reset(1280, 720, 123);
    another_seed.step(.02f, {}, 0);
    assert(std::abs(another_seed.particles()[0].x - first_x) > 1);
    const auto emitted = emitter.active_count();
    for (int turn = 0; turn < 4; ++turn) {
        emitter.resize(turn % 2 == 0 ? 720 : 1280, turn % 2 == 0 ? 1280 : 720, 1);
        assert(emitter.active_count() == emitted);
        bounds(emitter);
    }
    const float axes[4][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (int rotation = 0; rotation < 4; ++rotation) {
        float x = axes[rotation][0], y = axes[rotation][1];
        HomeScene::orient_vector(x, y, rotation);
        assert(x == 0 && y == 1);
    }
    for (const auto size : {std::pair<int,int>{1280,720}, {720,1280}, {420,280}, {280,160}}) {
        HomeScene scene;
        scene.reset(size.first, size.second);
        const float initial_y = mean_y(scene);
        for (int frame = 0; frame < 1200; ++frame) {
            scene.step(.02f, MotionSample{}, 0);
            bounds(scene);
        }
        assert(mean_y(scene) > initial_y);
        float speed = 0;
        for (const Particle& p : scene.particles()) speed = std::max(speed, std::abs(p.vx) + std::abs(p.vy));
        std::printf("%dx%d: 150 partículas, velocidad máxima en reposo %.2f px/s\n", size.first, size.second, speed);
        assert(speed < 50);
    }
    HomeScene left, right, up, neutral;
    left.reset(1280,720); right.reset(1280,720); up.reset(1280,720); neutral.reset(1280,720);
    for (int frame = 0; frame < 100; ++frame) {
        left.step(.02f, MotionSample{.accel_x=-1, .valid=true}, 0);
        right.step(.02f, MotionSample{.accel_x=1, .valid=true}, 0);
        up.step(.02f, MotionSample{.accel_y=-1, .valid=true}, 0);
        neutral.step(.02f, {}, 0);
    }
    assert(mean_x(left) < mean_x(neutral) && mean_x(right) > mean_x(neutral));
    assert(mean_y(up) < mean_y(neutral));
    HomeScene touched, baseline;
    touched.reset(320,200); baseline.reset(320,200);
    touched.step(.02f, {}, 0); baseline.step(.02f, {}, 0);
    const Particle p = touched.particles()[0];
    touched.perturb(p.x,p.y);
    touched.step(.02f, {}, 0); baseline.step(.02f, {}, 0);
    assert(std::abs(touched.particles()[0].x - baseline.particles()[0].x) > .1f);
    HomeScene shake;
    shake.reset(1280,720);
    const MotionSample strong{.accel_z=2.8f, .valid=true};
    shake.step(.02f,strong,0);
    assert(shake.phase() == ParticlePhase::Disappearing && shake.shake_count() == 1);
    bool empty = false;
    for (int frame=0; frame<70; ++frame) { shake.step(.02f,strong,0); empty |= shake.visibility() == 0; }
    assert(empty && shake.phase() == ParticlePhase::Visible && shake.shake_count() == 1);
    for (int frame=0; frame<80; ++frame) shake.step(.02f,{},0);
    shake.step(.02f,strong,0);
    assert(shake.shake_count() == 2);
    shake.step(.02f, MotionSample{.accel_x=std::numeric_limits<float>::quiet_NaN(), .valid=true},0);
    bounds(shake);
    assert(shake.active_count() < MAX_PARTICLES); // Regeneration also enters gradually.
    std::puts("OK: emisión gradual/aleatoria superior, giro sin reiniciar, gravedad, bordes, reposo, toque y sacudida.");
}
