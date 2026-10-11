#include "audio.hpp"
#include "bsp/m5stack_tab5.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace el_sistema;
namespace {
bool codec_ok = true, fail_write = false, partial_write = false;
std::vector<int16_t> output;
esp_err_t read_stub(void*, size_t, size_t* n, uint32_t) { *n = 0; return -1; }
esp_err_t write_stub(void* p, size_t size, size_t* n, uint32_t) {
    assert(audio_test_allocations == 1); // conversion buffer is heap-backed
    if (fail_write) { *n = 0; return -1; }
    if (partial_write) { *n = size / 2; return ESP_OK; }
    auto* samples = static_cast<int16_t*>(p);
    output.insert(output.end(), samples, samples + size / sizeof(int16_t));
    *n = size;
    return ESP_OK;
}
bsp_codec_config_t codec{read_stub, write_stub, nullptr};
}
bool bsp_codec_try_init() { return codec_ok; }
bool bsp_codec_ready() { return codec_ok; }
bsp_codec_config_t* bsp_get_codec_handle() { return &codec; }
int main() {
    AudioService audio;
    std::vector<int16_t> input(513);
    for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<int16_t>(i * 60 - 15000);
    codec_ok = false;
    assert(!audio.play_pcm(input.data(), input.size(), 16000));
    assert(audio.last_error().find("codecs") != std::string::npos);
    codec_ok = true;
    for (int cycle = 0; cycle < 10; ++cycle) {
        output.clear();
        assert(audio.play_pcm(input.data(), input.size(), 16000));
        assert(output.size() == input.size() * 6);
        for (size_t i = 0; i < input.size(); ++i)
            for (int j = 0; j < 6; ++j) assert(output[i * 6 + j] == input[i]);
        assert(audio_test_allocations == 0);
    }
    audio_test_fail_allocation = true;
    assert(!audio.play_pcm(input.data(), input.size(), 16000));
    assert(audio.last_error().find("PSRAM") != std::string::npos);
    assert(audio_test_allocations == 0);
    audio_test_fail_allocation = false;
    fail_write = true;
    assert(!audio.play_pcm(input.data(), input.size(), 16000));
    assert(audio_test_allocations == 0);
    fail_write = false;
    partial_write = true;
    assert(!audio.play_pcm(input.data(), input.size(), 16000));
    assert(audio_test_allocations == 0);
    partial_write = false;
    std::atomic<bool> cancel{true};
    assert(!audio.play_pcm(input.data(), input.size(), 16000, &cancel));
    assert(audio_test_allocations == 0);
    assert(!audio.play_pcm(nullptr, 0, 16000));
    assert(!audio.play_pcm(input.data(), input.size(), 48000));
    std::puts("Audio PCM: conversión estéreo, diez ciclos, cancelación, fallo de codec/PSRAM/I2S y limpieza OK");
}
