#include "hourglass_model.hpp"
#include "hourglass_raster.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <vector>

using namespace el_sistema;
static MotionSample down() { return MotionSample{.accel_y = 1, .valid = true}; }
static MotionSample up() { return MotionSample{.accel_y = -1, .valid = true}; }
static void conserved(const HourglassSnapshot& state)
{
    assert(std::isfinite(state.upper_mass) && std::isfinite(state.lower_mass));
    assert(state.upper_mass >= 0 && state.upper_mass <= 1);
    assert(std::abs(state.upper_mass + state.lower_mass - 1) < 1e-12);
}
static void ppm(const char* path, const HourglassSnapshot& snapshot)
{
    constexpr int width = 480, height = 1040;
    HourglassRaster raster;
    assert(raster.init(width, height));
    std::vector<uint16_t> pixels(width * height);
    raster.draw(pixels.data(), snapshot);
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << width << " " << height << "\n255\n";
    for (uint16_t pixel : pixels) {
        const char rgb[] = {char(((pixel >> 11) & 31) * 255 / 31),
                            char(((pixel >> 5) & 63) * 255 / 63), char((pixel & 31) * 255 / 31)};
        file.write(rgb, sizeof(rgb));
    }
}
int main(int argc, char** argv)
{
    for (uint16_t minutes : {1, 25, 120}) {
        HourglassEngine clock;
        assert(clock.start(minutes, down(), 0));
        const int limit = minutes * 60 + 5;
        for (int second = 1; second <= limit && clock.snapshot().state == HourglassState::Running; ++second) {
            clock.step(int64_t(second) * 1000000, down());
            conserved(clock.snapshot());
        }
        const auto state = clock.snapshot();
        if (state.state != HourglassState::Completed) {
            std::fprintf(stderr, "Vaciado bloqueado: %u min, masa %.12f, umbral %.5f, cuello %.5f\n",
                         minutes, state.upper_mass, state.upper.threshold, state.upper.score(0, 0.005f));
        }
        assert(state.state == HourglassState::Completed);
        const double actual = state.elapsed_us / 1000000.0;
        assert(std::abs(actual - minutes * 60) <= std::max(1.0, minutes * 0.6));
        assert(state.upper_mass < 1e-9 && state.lower_mass > 0.999999);
        std::printf("%u min: vaciado en %.2f s, conservación correcta\n", minutes, actual);
    }
    HourglassEngine clock;
    assert(!clock.start(0, down(), 0));
    assert(!clock.start(121, down(), 0));
    assert(!clock.start(1, MotionSample{}, 0));
    assert(!clock.start(1, MotionSample{.accel_x = 1, .valid = true}, 0));
    assert(!clock.start(1, MotionSample{.accel_z = 1, .valid = true}, 0));
    assert(clock.start(1, down(), 0));
    clock.step(15000000, down());
    const auto quarter = clock.snapshot();
    assert(std::abs(quarter.upper_mass - 0.75) < 0.001);
    clock.pause(15000000);
    clock.step(1500000000, up());
    assert(clock.snapshot().elapsed_us == quarter.elapsed_us);
    assert(clock.snapshot().upper_mass == quarter.upper_mass);
    assert(clock.snapshot().gravity_y == quarter.gravity_y);
    assert(clock.resume(down(), 1500000000));
    clock.step(1501000000, down());
    assert(clock.snapshot().elapsed_us == quarter.elapsed_us + 1000000);

    // Turning mid-run reverses transfer without resetting time or refilling.
    const double before_reverse = clock.snapshot().upper_mass;
    for (int i = 1; i <= 90; ++i) clock.step(1501000000 + i * 20000, up());
    assert(clock.snapshot().upper_mass > before_reverse);
    assert(clock.snapshot().elapsed_us > quarter.elapsed_us);
    assert(clock.snapshot().state == HourglassState::Running);
    conserved(clock.snapshot());
    // Flat means no made-up in-plane force. Once the filter settles, no flow.
    int64_t now = 1503000000;
    const MotionSample flat{.accel_z = 1, .valid = true};
    for (int i = 0; i < 120; ++i) { now += 20000; clock.step(now, flat); }
    const double flat_mass = clock.snapshot().upper_mass;
    const uint64_t flat_elapsed = clock.snapshot().elapsed_us;
    const float flat_surface_y = clock.snapshot().lower.gy;
    now += 1000000; clock.step(now, flat);
    assert(clock.snapshot().upper_mass == flat_mass);
    assert(clock.snapshot().elapsed_us == flat_elapsed + 1000000);
    assert(clock.snapshot().lower.gy == flat_surface_y);
    clock.step(now + 20000, MotionSample{});
    assert(clock.snapshot().state == HourglassState::Paused);
    assert(clock.snapshot().error == HourglassError::SensorUnavailable);
    assert(!clock.resume(MotionSample{}, now + 100000));

    HourglassRecord bytes = encode_hourglass(clock.snapshot());
    HourglassSnapshot recovered;
    assert(decode_hourglass(bytes, recovered));
    HourglassEngine restored;
    restored.restore(recovered, now + 10000000000);
    assert(restored.snapshot().state == HourglassState::Paused);
    assert(std::abs(restored.snapshot().upper_mass - clock.snapshot().upper_mass) < 1e-6);
    assert(restored.snapshot().elapsed_us == clock.snapshot().elapsed_us);
    bytes[16] ^= 1;
    assert(!decode_hourglass(bytes, recovered));
    bytes = encode_hourglass(quarter);
    assert(decode_hourglass(bytes, recovered));
    restored.restore(recovered, 9000000000);
    restored.step(19000000000, down());
    assert(restored.snapshot().state == HourglassState::Paused);
    assert(restored.snapshot().elapsed_us == quarter.elapsed_us);

    HourglassEngine complete;
    assert(complete.start(1, down(), 0));
    for (int i = 1; i <= 65; ++i) complete.step(i * 1000000LL, down());
    const uint64_t final_time = complete.snapshot().elapsed_us;
    complete.step(66000000, down());
    assert(complete.snapshot().elapsed_us == final_time);
    int64_t reverse = 66000000;
    for (int i = 0; i < 10; ++i) { reverse += 20000; complete.step(reverse, up()); }
    complete.step(reverse + 20000, down());
    assert(complete.snapshot().state == HourglassState::Completed);
    reverse += 20000;
    for (int i = 0; i < 90; ++i) { reverse += 20000; complete.step(reverse, up()); }
    assert(complete.snapshot().state == HourglassState::Running);
    assert(complete.snapshot().elapsed_us < 1000000);
    assert(complete.snapshot().upper_mass > 0);
    const auto auto_reversed = complete.snapshot();
    complete.step(reverse + 20000, up());
    assert(complete.snapshot().elapsed_us > auto_reversed.elapsed_us);
    // Shake never destroys mass or restarts an active measurement.
    complete.step(reverse + 40000, MotionSample{.accel_y = -3, .valid = true});
    conserved(complete.snapshot());
    assert(complete.snapshot().elapsed_us > auto_reversed.elapsed_us);
    assert(complete.snapshot().state == HourglassState::Running);
    complete.step(reverse + 60000, MotionSample{.accel_x = std::numeric_limits<float>::quiet_NaN(), .valid = true});
    assert(complete.snapshot().state == HourglassState::Paused);

    HourglassEngine inverted;
    assert(inverted.start(1, up(), 0));
    assert(inverted.snapshot().lower_mass == 1 && inverted.snapshot().upper_mass == 0);
    // The compositor may draw the glass outside the cavity, but emitted light
    // must remain clipped inside it; canaries also cover full-buffer writes.
    constexpr int width = 480, height = 1040;
    HourglassRaster raster;
    assert(raster.init(width, height));
    std::vector<uint16_t> guarded(width * height + 32, 0xA55A);
    raster.draw(guarded.data() + 16, quarter);
    for (int i = 0; i < 16; ++i) {
        assert(guarded[i] == 0xA55A && guarded[width * height + 16 + i] == 0xA55A);
    }
    for (int y = 60; y < 980; ++y) {
        const float world_y = (y - height * 0.5f) / ((height - 100) * 0.5f);
        for (int x = 0; x < width; ++x) {
            const uint16_t pixel = guarded[16 + y * width + x];
            if (!pixel || pixel == light_rgb565(theme::HourglassGlass) ||
                pixel == light_rgb565(theme::HourglassReflection)) continue;
            assert(HourglassScene::contains((x - width * 0.5f) / (width * 0.67f), world_y));
        }
    }
    if (argc > 1) {
        HourglassEngine preview;
        assert(preview.start(1, down(), 0));
        preview.step(30000000, down());
        ppm(argv[1], preview.snapshot());
    }
    std::puts("OK: inclinación, inversión, pausa, sensor, recuperación, corrupción y reinicio físico estable.");
}
