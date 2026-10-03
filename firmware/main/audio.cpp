#include "audio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "bsp/m5stack_tab5.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "audio";
constexpr uint32_t kNativeRate = 48000;
constexpr uint32_t kOutputRate = 16000;
constexpr size_t kSlots = 4;
constexpr uint32_t kChunkMs = 20;
constexpr size_t kFramesPerChunk = kNativeRate * kChunkMs / 1000;
constexpr size_t kRawSamplesPerChunk = kFramesPerChunk * kSlots;
constexpr size_t kOutputSamplesPerChunk = kOutputRate * kChunkMs / 1000;
constexpr size_t kMaxOutputSamples = kOutputRate * 30;
constexpr uint32_t kNoVoiceMs = 5000;
constexpr uint32_t kTrailingSilenceMs = 3000;
constexpr uint32_t kVoiceAttackMs = 60;
// Calibración física 2026-10-01: silencio RMS 0.00027-0.00031 / pico
// 0.00107-0.00122; voz RMS 0.00145-0.00157 / pico 0.00345-0.00360.
constexpr float kVoiceStartRms = 0.00070f;
constexpr float kVoiceStartPeak = 0.00180f;
constexpr float kVoiceHoldRms = 0.00048f;
constexpr float kVoiceHoldPeak = 0.00135f;
constexpr size_t kMicrophoneSlotA = 0;
constexpr size_t kMicrophoneSlotB = 2;
constexpr size_t kReferenceSlot = 3;
constexpr std::array<int16_t, 15> kDownsampleFir = {
    37, -128, -525, -667, 687, 4080, 8012, 9776, 8012, 4080, 687, -667, -525, -128, 37,
};

void put_u16(uint8_t* target, uint16_t value)
{
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
}

void put_u32(uint8_t* target, uint32_t value)
{
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
    target[2] = static_cast<uint8_t>(value >> 16);
    target[3] = static_cast<uint8_t>(value >> 24);
}

}  // namespace

void Downsampler48To16::reset()
{
    history_.fill(0);
    phase_ = 0;
}

bool Downsampler48To16::push(int16_t input, int16_t& output)
{
    for (size_t index = history_.size() - 1; index > 0; --index) history_[index] = history_[index - 1];
    history_[0] = input;
    phase_ = static_cast<uint8_t>((phase_ + 1) % 3);
    if (phase_ != 0) return false;
    int64_t accumulator = 0;
    for (size_t index = 0; index < history_.size(); ++index) {
        accumulator += static_cast<int32_t>(history_[index]) * kDownsampleFir[index];
    }
    const int32_t filtered = std::clamp<int32_t>(static_cast<int32_t>(accumulator >> 15), -32768, 32767);
    output = static_cast<int16_t>(filtered);
    return true;
}

void AudioBuffer::Deleter::operator()(int16_t* pointer) const { heap_caps_free(pointer); }
void WavBuffer::Deleter::operator()(uint8_t* pointer) const { heap_caps_free(pointer); }

void AudioBuffer::clear()
{
    samples.reset();
    sample_count = 0;
}

VoiceDecision VoiceActivityDetector::update(float rms, float peak, uint32_t elapsed_ms)
{
    if (!voice_seen_) {
        const bool candidate = rms >= kVoiceStartRms || peak >= kVoiceStartPeak;
        candidate_ms_ = candidate ? candidate_ms_ + kChunkMs : 0;
        if (candidate_ms_ >= kVoiceAttackMs) {
            voice_seen_ = true;
            silence_ms_ = 0;
            return VoiceDecision::Speaking;
        }
        return elapsed_ms >= kNoVoiceMs ? VoiceDecision::StopForNoVoice : VoiceDecision::Waiting;
    }

    const bool speaking = rms >= kVoiceHoldRms || peak >= kVoiceHoldPeak;
    if (speaking) {
        silence_ms_ = 0;
        return VoiceDecision::Speaking;
    }
    silence_ms_ += kChunkMs;
    return silence_ms_ >= kTrailingSilenceMs ? VoiceDecision::StopForSilence : VoiceDecision::Waiting;
}

bool AudioService::init()
{
    if (available_) return true;
    if (!bsp_codec_try_init() || !bsp_codec_ready()) {
        last_error_ = "No fue posible iniciar los codecs ES7210/ES8388";
        ESP_LOGW(TAG, "%s; el resto de la aplicación continuará", last_error_.c_str());
        return false;
    }
    bsp_codec_config_t* codec = bsp_get_codec_handle();
    if (codec == nullptr || codec->i2s_read == nullptr || codec->i2s_write == nullptr) {
        last_error_ = "El BSP no expuso la ruta de audio";
        return false;
    }
    if (codec->set_in_gain != nullptr) codec->set_in_gain(24.0f);
    available_ = true;
    last_error_.clear();
    ESP_LOGI(TAG, "Audio listo: TDM 48 kHz/16-bit/4 slots -> PCM mono 16 kHz");
    return true;
}

CaptureEnd AudioService::capture(AudioBuffer& output, std::atomic<bool>& stop_requested,
                                 std::atomic<bool>& cancel_requested, const LevelCallback& callback)
{
    output.clear();
    if (!available_ && !init()) return CaptureEnd::Error;

    auto* pcm = static_cast<int16_t*>(heap_caps_malloc(kMaxOutputSamples * sizeof(int16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    auto* raw = static_cast<int16_t*>(heap_caps_malloc(kRawSamplesPerChunk * sizeof(int16_t),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcm == nullptr || raw == nullptr) {
        heap_caps_free(pcm);
        heap_caps_free(raw);
        last_error_ = "PSRAM insuficiente para una grabación de 30 segundos";
        return CaptureEnd::Error;
    }

    output.samples.reset(pcm);
    output.sample_count = 0;
    output.sample_rate = kOutputRate;
    bsp_codec_config_t* codec = bsp_get_codec_handle();
    CaptureEnd end = CaptureEnd::Limit;
    Downsampler48To16 downsampler;
    VoiceActivityDetector voice_detector;

    while (output.sample_count + kOutputSamplesPerChunk <= kMaxOutputSamples) {
        if (cancel_requested.load()) {
            end = CaptureEnd::Cancelled;
            break;
        }
        if (stop_requested.load()) {
            end = CaptureEnd::Stopped;
            break;
        }

        size_t bytes_read = 0;
        const esp_err_t err = codec->i2s_read(raw, kRawSamplesPerChunk * sizeof(int16_t), &bytes_read, 1000);
        if (err != ESP_OK || bytes_read != kRawSamplesPerChunk * sizeof(int16_t)) {
            last_error_ = "La captura I2S devolvió un bloque incompleto";
            end = CaptureEnd::Error;
            break;
        }

        AudioLevel level;
        std::array<double, kSlots> squares {};
        std::array<int32_t, kSlots> peaks {};
        std::array<int16_t, kSlots> minimums {INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX};
        std::array<int16_t, kSlots> maximums {INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN};
        std::array<uint16_t, kSlots> nonzero {};
        for (size_t frame = 0; frame < kFramesPerChunk; ++frame) {
            const size_t base = frame * kSlots;
            for (size_t slot = 0; slot < kSlots; ++slot) {
                const int16_t sample = raw[base + slot];
                const int32_t value = sample;
                squares[slot] += static_cast<double>(value) * value;
                peaks[slot] = std::max(peaks[slot], std::abs(value));
                minimums[slot] = std::min(minimums[slot], sample);
                maximums[slot] = std::max(maximums[slot], sample);
                if (sample != 0) ++nonzero[slot];
            }
        }

        double mixed_squares = 0.0;
        int32_t mixed_peak = 0;
        size_t out_index = output.sample_count;
        for (size_t frame = 0; frame < kFramesPerChunk; ++frame) {
            const size_t base = frame * kSlots;
            const int32_t a = raw[base + kMicrophoneSlotA];
            const int32_t b = raw[base + kMicrophoneSlotB];
            const int16_t mono48 = static_cast<int16_t>(std::clamp<int32_t>((a + b) / 2, -32768, 32767));
            int16_t mixed = 0;
            if (!downsampler.push(mono48, mixed)) continue;
            pcm[out_index++] = mixed;
            mixed_squares += static_cast<double>(mixed) * mixed;
            mixed_peak = std::max(mixed_peak, std::abs(static_cast<int32_t>(mixed)));
        }
        output.sample_count = out_index;

        level.elapsed_ms = output.duration_ms();
        level.rms = static_cast<float>(std::sqrt(mixed_squares / kOutputSamplesPerChunk) / 32768.0);
        level.peak = static_cast<float>(mixed_peak / 32768.0);
        for (size_t slot = 0; slot < kSlots; ++slot) {
            level.slot_rms[slot] = static_cast<float>(std::sqrt(squares[slot] / kFramesPerChunk) / 32768.0);
            level.slot_peak[slot] = static_cast<float>(peaks[slot] / 32768.0);
            level.slot_min[slot] = minimums[slot];
            level.slot_max[slot] = maximums[slot];
            level.slot_nonzero[slot] = nonzero[slot];
        }
        level.bytes_read = static_cast<uint32_t>(bytes_read);
        level.microphone_slot_a = static_cast<uint8_t>(kMicrophoneSlotA);
        level.microphone_slot_b = static_cast<uint8_t>(kMicrophoneSlotB);
        level.reference_slot = static_cast<uint8_t>(kReferenceSlot);
        if (callback) callback(level);

        // La deteccion se calibro contra cada slot fisico. No usamos aqui la
        // mezcla estéreo: si ambos micrófonos reciben la misma voz con un
        // pequeño desfase, promediarlos puede cancelar parte de la señal y
        // provocar un falso "silencio".
        const float voice_rms = std::max(level.slot_rms[kMicrophoneSlotA],
                                         level.slot_rms[kMicrophoneSlotB]);
        const float voice_peak = std::max(level.slot_peak[kMicrophoneSlotA],
                                          level.slot_peak[kMicrophoneSlotB]);
        const VoiceDecision decision = voice_detector.update(voice_rms, voice_peak, level.elapsed_ms);
        if (decision == VoiceDecision::StopForNoVoice) {
            end = CaptureEnd::NoVoice;
            break;
        }
        if (decision == VoiceDecision::StopForSilence) {
            end = CaptureEnd::Silence;
            break;
        }
    }

    heap_caps_free(raw);
    if (end == CaptureEnd::Cancelled || end == CaptureEnd::NoVoice || end == CaptureEnd::Error) output.clear();
    ESP_LOGI(TAG, "Captura terminada: motivo=%d, muestras=%u", static_cast<int>(end),
             static_cast<unsigned>(output.sample_count));
    return end;
}

bool AudioService::play(const AudioBuffer& audio, const std::atomic<bool>* cancel_requested)
{
    if (audio.empty()) return false;
    return play_pcm(audio.samples.get(), audio.sample_count, audio.sample_rate, cancel_requested);
}

bool AudioService::play_pcm(const int16_t* samples, size_t sample_count, uint32_t sample_rate,
                            const std::atomic<bool>* cancel_requested)
{
    if (samples == nullptr || sample_count == 0 || sample_rate != 16000 || (!available_ && !init())) {
        last_error_ = sample_rate == 16000 ? "No hay PCM para reproducir"
                                           : "La reproducción requiere PCM mono de 16 kHz";
        return false;
    }
    bsp_codec_config_t* codec = bsp_get_codec_handle();
    constexpr size_t kInputChunk = 256;
    int16_t stereo48[kInputChunk * 3 * 2];
    for (size_t offset = 0; offset < sample_count; offset += kInputChunk) {
        if (cancel_requested != nullptr && cancel_requested->load()) return false;
        const size_t count = std::min(kInputChunk, sample_count - offset);
        size_t cursor = 0;
        for (size_t i = 0; i < count; ++i) {
            const int16_t value = samples[offset + i];
            for (int repeat = 0; repeat < 3; ++repeat) {
                stereo48[cursor++] = value;
                stereo48[cursor++] = value;
            }
        }
        size_t written = 0;
        if (codec->i2s_write(stereo48, cursor * sizeof(int16_t), &written, 1000) != ESP_OK ||
            written != cursor * sizeof(int16_t)) {
            last_error_ = "Falló la reproducción de la muestra";
            return false;
        }
    }
    return true;
}

bool AudioService::make_wav(const AudioBuffer& audio, WavBuffer& wav, std::string& error)
{
    wav.bytes.reset();
    wav.size = 0;
    if (audio.empty() || audio.sample_rate == 0) {
        error = "No hay PCM para convertir";
        return false;
    }
    const size_t pcm_bytes = audio.sample_count * sizeof(int16_t);
    if (pcm_bytes > std::numeric_limits<uint32_t>::max() - 44) {
        error = "La muestra es demasiado grande";
        return false;
    }
    auto* bytes = static_cast<uint8_t*>(heap_caps_malloc(44 + pcm_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (bytes == nullptr) {
        error = "PSRAM insuficiente para construir el WAV";
        return false;
    }
    std::memcpy(bytes, "RIFF", 4);
    put_u32(bytes + 4, static_cast<uint32_t>(36 + pcm_bytes));
    std::memcpy(bytes + 8, "WAVEfmt ", 8);
    put_u32(bytes + 16, 16);
    put_u16(bytes + 20, 1);
    put_u16(bytes + 22, 1);
    put_u32(bytes + 24, audio.sample_rate);
    put_u32(bytes + 28, audio.sample_rate * 2);
    put_u16(bytes + 32, 2);
    put_u16(bytes + 34, 16);
    std::memcpy(bytes + 36, "data", 4);
    put_u32(bytes + 40, static_cast<uint32_t>(pcm_bytes));
    std::memcpy(bytes + 44, audio.samples.get(), pcm_bytes);
    wav.bytes.reset(bytes);
    wav.size = 44 + pcm_bytes;
    error.clear();
    return true;
}

}  // namespace el_sistema
