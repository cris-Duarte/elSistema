#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace el_sistema {

struct AudioLevel {
    uint32_t elapsed_ms = 0;
    float rms = 0.0f;
    float peak = 0.0f;
    std::array<float, 4> slot_rms {};
    std::array<float, 4> slot_peak {};
    std::array<int16_t, 4> slot_min {};
    std::array<int16_t, 4> slot_max {};
    std::array<uint16_t, 4> slot_nonzero {};
    uint32_t bytes_read = 0;
    uint8_t microphone_slot_a = 0;
    uint8_t microphone_slot_b = 1;
    uint8_t reference_slot = 2;
};

struct AudioBuffer {
    struct Deleter {
        void operator()(int16_t* pointer) const;
    };
    std::unique_ptr<int16_t, Deleter> samples;
    size_t sample_count = 0;
    uint32_t sample_rate = 16000;

    void clear();
    bool empty() const { return samples == nullptr || sample_count == 0; }
    uint32_t duration_ms() const { return static_cast<uint32_t>(sample_count * 1000ULL / sample_rate); }
};

struct WavBuffer {
    struct Deleter {
        void operator()(uint8_t* pointer) const;
    };
    std::unique_ptr<uint8_t, Deleter> bytes;
    size_t size = 0;
};

enum class CaptureEnd : uint8_t { Stopped, Silence, NoVoice, Limit, Cancelled, Error };
enum class VoiceDecision : uint8_t { Waiting, Speaking, StopForSilence, StopForNoVoice };

// Detector calibrado con los dos micrófonos físicos del Tab5. La histéresis
// permite conservar palabras suaves sin confundir el ruido estable con voz.
class VoiceActivityDetector {
public:
    VoiceDecision update(float rms, float peak, uint32_t elapsed_ms);
    bool voice_seen() const { return voice_seen_; }

private:
    bool voice_seen_ = false;
    uint32_t candidate_ms_ = 0;
    uint32_t silence_ms_ = 0;
};

// FIR de 15 taps a 7,2 kHz y decimacion 3:1. Es independiente del hardware
// para poder comprobarlo con señales sinteticas en las pruebas de Unity.
class Downsampler48To16 {
public:
    void reset();
    bool push(int16_t input, int16_t& output);

private:
    std::array<int16_t, 15> history_ {};
    uint8_t phase_ = 0;
};

// Captura TDM nativa del Tab5 y entrega PCM mono de 16 kHz. Nunca llama a LVGL.
class AudioService {
public:
    using LevelCallback = std::function<void(const AudioLevel&)>;

    bool init();
    bool available() const { return available_; }
    const std::string& last_error() const { return last_error_; }

    CaptureEnd capture(AudioBuffer& output, std::atomic<bool>& stop_requested,
                       std::atomic<bool>& cancel_requested, const LevelCallback& callback);
    bool play(const AudioBuffer& audio, const std::atomic<bool>* cancel_requested = nullptr);
    bool play_pcm(const int16_t* samples, size_t sample_count, uint32_t sample_rate,
                  const std::atomic<bool>* cancel_requested = nullptr);
    static bool make_wav(const AudioBuffer& audio, WavBuffer& wav, std::string& error);

private:
    bool available_ = false;
    std::string last_error_;
};

}  // namespace el_sistema
