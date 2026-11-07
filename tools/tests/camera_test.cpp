#include "camera_model.hpp"
#include "modules.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace el_sistema;
static void segment(std::vector<uint8_t>& bytes, uint8_t marker, const std::vector<uint8_t>& payload)
{
    const size_t len = payload.size() + 2;
    bytes.insert(bytes.end(), {0xff, marker, uint8_t(len >> 8), uint8_t(len)});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}
// Structurally valid header fixture, not a decodable Sony image. Hardware
// decoding, real entropy data and Sony color fidelity require physical tests.
static std::vector<uint8_t> baseline_header(bool tables = true)
{
    std::vector<uint8_t> bytes{0xff, 0xd8}, table(65, 1);
    table[0] = 0; segment(bytes, 0xdb, table);
    table[0] = 1; segment(bytes, 0xdb, table);
    segment(bytes, 0xc0, {8, 2, 208, 5, 0, 3, 1, 0x22, 0, 2, 0x11, 1, 3, 0x11, 1});
    if (tables) for (uint8_t id : {0, 1, 0x10, 0x11}) {
        std::vector<uint8_t> huffman(18, 0); huffman[0] = id; huffman[1] = 1;
        segment(bytes, 0xc4, huffman);
    }
    segment(bytes, 0xda, {3, 1, 0, 2, 0x11, 3, 0x11, 0, 63, 0});
    bytes.insert(bytes.end(), {0x10, 0x20, 0xff, 0xd9});
    return bytes;
}
int main()
{
    assert(kModules.size() == 5 && static_cast<int>(ModuleId::Camera) == 10);
    for (int id : {2,3,5,7,8,9}) assert(!find_module(static_cast<ModuleId>(id)));
    CameraIntervals discrete;
    discrete.count = 3; discrete.discrete = {666666, 400000, 333333};
    assert(std::abs(camera_choose_fps(discrete)-30) < .001f);
    discrete.count = 2; assert(camera_choose_fps(discrete) == 25.f);
    discrete.count = 1; assert(camera_choose_fps(discrete) == 0);
    discrete.discrete[0] = 333667; assert(std::abs(camera_choose_fps(discrete)-29.97f) < .01f);
    CameraIntervals continuous;
    continuous.minimum = 300000; continuous.maximum = 500000; continuous.step = 100000;
    assert(camera_choose_fps(continuous) == 25.f);
    continuous.minimum = 1; continuous.maximum = 1000000; continuous.step = 0;
    assert(std::abs(camera_choose_fps(continuous)-30) < .001f);
    assert(!camera_choose_fps({}));
    assert(!camera_frame_fresh(0, 100));
    assert(!camera_frame_fresh(100, 99));
    assert(camera_frame_fresh(100, 100));
    assert(camera_frame_fresh(100, 1000099));
    assert(!camera_frame_fresh(100, 1000100));

    CameraFrameSlots pool;
    int a = pool.begin_write(); assert(a >= 0 && !pool.idle());
    pool.publish(a, 1, 1, 10); assert(pool.idle());
    assert(pool.acquire_latest(2) == -1); // Old-session result cannot cross a reconnect.
    assert(pool.discarded() == 1);
    a = pool.begin_write(); pool.publish(a, 2, 2, 20);
    const int displayed = pool.acquire_latest(2); assert(displayed == a && !pool.idle());
    int b = pool.begin_write(); assert(b >= 0 && b != displayed);
    assert(pool.begin_write() == -1); // Both leased: never write into LVGL's source.
    pool.publish(b, 2, 3, 30);
    const int overwrite = pool.begin_write(); assert(overwrite == b);
    assert(pool.discarded() == 2);
    pool.publish(overwrite, 2, 4, 40);
    assert(pool.sequence(displayed) == 2);
    const int next = pool.acquire_latest(2); assert(next == b && pool.sequence(next) == 4);
    assert(pool.begin_write() == -1);
    pool.reset(); assert(pool.begin_write() == -1); // Reset cannot revoke a display lease.
    pool.release(displayed); assert(pool.begin_write() == displayed);
    pool.abort_write(displayed); pool.release(next); assert(pool.idle());
    pool.reset(); assert(pool.acquire_latest(2) == -1);
    for (uint32_t i = 1; i <= 10000; ++i) {
        a = pool.begin_write(); assert(a >= 0); pool.publish(a, 3, i, 100+i);
        a = pool.acquire_latest(3); assert(a >= 0 && pool.sequence(a) == i);
        assert(pool.timestamp(a) == 100+i); pool.release(a);
    }

    auto jpeg = baseline_header();
    assert(camera_valid_jpeg(jpeg.data(), jpeg.size()));
    auto no_huffman = baseline_header(false);
    assert(camera_valid_jpeg(no_huffman.data(), no_huffman.size()));
    for (size_t length = 0; length < jpeg.size(); ++length) assert(!camera_valid_jpeg(jpeg.data(), length));
    assert(!camera_valid_jpeg(nullptr, 100));
    auto bad = jpeg; bad[4] = 0xff; bad[5] = 0xff;
    assert(!camera_valid_jpeg(bad.data(), bad.size())); // Segment beyond input.
    bad = jpeg; bad[7] = 0; assert(!camera_valid_jpeg(bad.data(), bad.size())); // Invalid quantization.
    for (size_t i = 2; i+1 < jpeg.size(); ++i) {
        if (jpeg[i] == 0xff && jpeg[i+1] == 0xc0) {
            bad = jpeg; bad[i+1] = 0xc2; assert(!camera_valid_jpeg(bad.data(), bad.size())); // Progressive.
            bad = jpeg; bad[i+7] = 1; assert(!camera_valid_jpeg(bad.data(), bad.size())); // Wrong dimensions.
        }
        if (jpeg[i] == 0xff && jpeg[i+1] == 0xc4) {
            bad = jpeg; bad[i+5] = 250; assert(!camera_valid_jpeg(bad.data(), bad.size())); // Huffman table overrun.
        }
    }
    bad.assign(CAMERA_JPEG_LIMIT+1, 0);
    assert(!camera_valid_jpeg(bad.data(), bad.size()));
    // A complete, bounded JPEG must never be accepted based only on SOI/EOI.
    for (uint32_t seed = 1; seed < 2000; ++seed) {
        bad.resize(100); uint32_t random = seed;
        for (auto& v : bad) { random = random*1664525u+1013904223u; v = random >> 24; }
        bad[0] = 0xff; bad[1] = 0xd8; bad[98] = 0xff; bad[99] = 0xd9;
        assert(!camera_valid_jpeg(bad.data(), bad.size()));
    }
    std::puts("Cámara: formatos, caducidad, sesiones, buffers y límites JPEG OK. Pendiente hardware Sony.");
}
