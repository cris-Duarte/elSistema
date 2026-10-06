#include "device_state.hpp"
#include "light_particle.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace el_sistema;

static MotionSample orientation(int rotation)
{
    MotionSample sample{};
    sample.valid = true;
    if (rotation == 0) sample.accel_y = 1;
    if (rotation == 1) sample.accel_x = 1;
    if (rotation == 2) sample.accel_y = -1;
    if (rotation == 3) sample.accel_x = -1;
    return sample;
}

int main()
{
    assert(ina226_battery_mv(6400) == 8000);
    assert(ina226_battery_mv(6720) == 8400);
    assert(ina226_battery_mv(0) == 0);
    assert(ina226_battery_mv(0x8000) == -1);
    assert(battery_percent_from_mv(1000) == -1);
    assert(battery_percent_from_mv(9200) == -1);
    assert(battery_percent_from_mv(6600) == 0);
    assert(battery_percent_from_mv(7400) == 50);
    assert(battery_percent_from_mv(8000) == 87);
    assert(battery_percent_from_mv(8200) == 100);
    assert(battery_percent_from_mv(8400) == 100);

    for (int from = 0; from < 4; ++from) {
        for (int to = 0; to < 4; ++to) {
            OrientationDetector detector(from);
            assert(detector.update(orientation(to), 1000000) == from);
            assert(detector.update(orientation(to), 1799999) == from);
            assert(detector.update(orientation(to), 1800000) == to);
            // Detection remains available after a UI lock, without consuming
            // a one-shot event while the keyboard/dialog is open.
            assert(detector.update(orientation(to), 10000000) == to);
            const int turns = shortest_rotation_turns(from, to);
            assert(turns >= -1 && turns <= 2);
            assert(((from + turns + 4) & 3) == to);
        }
    }
    OrientationDetector detector;
    assert(detector.update(orientation(0), 0) == 1);
    assert(detector.update({}, 700000) == 1); // Invalid reading resets candidate.
    assert(detector.update(orientation(0), 800000) == 1);
    assert(detector.update(orientation(0), 1599999) == 1);
    assert(detector.update(orientation(0), 1600000) == 0);
    assert(detector.update(MotionSample{.accel_z=1, .valid=true}, 2000000) == 0);
    assert(detector.update(MotionSample{.accel_x=3, .valid=true}, 3000000) == 0);
    assert(detector.update(MotionSample{.accel_x=3, .valid=true}, 4000000) == 0);
    assert(detector.update(MotionSample{.accel_x=std::numeric_limits<float>::quiet_NaN(), .valid=true}, 5000000) == 0);
    const MotionSample diagonal{.accel_x=.72f, .accel_y=.70f, .valid=true};
    assert(detector.update(diagonal, 6000000) == 0);
    assert(detector.update(diagonal, 7000000) == 0); // Axis hysteresis.
    const MotionSample tilted{.accel_x=-.35f, .accel_z=.94f, .valid=true};
    assert(detector.update(tilted, 8000000) == 0);
    assert(detector.update(tilted, 8800000) == 3);

    const ScreenPoint original{120, 200};
    for (int turns = -1; turns <= 2; ++turns) {
        const ScreenPoint transformed = rotate_screen_point(original, 720, 1280, turns);
        const bool swapped = turns & 1;
        const int width = swapped ? 1280 : 720, height = swapped ? 720 : 1280;
        assert(transformed.x >= 0 && transformed.x <= width);
        assert(transformed.y >= 0 && transformed.y <= height);
        const ScreenPoint restored = rotate_screen_point(transformed, width, height, -turns);
        assert(restored.x == original.x && restored.y == original.y);
    }

    for (uint32_t tint : {theme::HomeParticleCyan, theme::HomeParticleLime,
                          theme::HomeParticleRose, theme::HomeParticleLavender}) {
        for (int radius = 1; radius <= LIGHT_MAX_RADIUS; ++radius) {
            LightSprite sprite;
            sprite.build(radius, tint);
            const int side = radius * 2 + 1;
            assert(sprite.radius == radius);
            assert(sprite.pixels[radius * side + radius] == light_rgb565(theme::HomeLightCore));
            assert(sprite.pixels[0] == 0 && sprite.pixels[radius * side] == 0);
            unsigned previous_r = 31, previous_g = 63, previous_b = 31;
            for (int x = 0; x <= radius; ++x) {
                const uint16_t value = sprite.pixels[radius * side + radius + x];
                assert(value == sprite.pixels[radius * side + radius - x]);
                const unsigned r = value >> 11, g = (value >> 5) & 63, b = value & 31;
                assert(r <= previous_r && g <= previous_g && b <= previous_b);
                previous_r = r; previous_g = g; previous_b = b;
            }
            for (int y = 0; y < side; ++y)
                for (int x = 0; x < side; ++x)
                    assert(sprite.pixels[y * side + x] == sprite.pixels[(side - y - 1) * side + x]);
        }
    }
    assert(add_light(0x1234, 0xFFFF, 0) == 0x1234);
    assert(add_light(0x1234, 0) == 0x1234);
    assert(add_light(0, 0xFFFF) == 0xFFFF);
    assert(add_light(0xFFFF, 0xFFFF) == 0xFFFF);
    assert(add_light(0xF800, 0x001F) == 0xF81F); // No carries between channels.
    assert(add_light(0, 0xFFFF, 128) != 0xFFFF);
    std::puts("OK: INA226/porcentaje, cuatro orientaciones estables, bloqueo de UI, giro de coordenadas y luz aditiva.");
}
