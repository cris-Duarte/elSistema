#pragma once

#include "audio.hpp"
#include "audio_session.hpp"
#include "dictation.hpp"

#include <atomic>
#include <cstdint>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace el_sistema {

enum class AssistantVoicePhase : uint8_t {
    Idle,
    Listening,
    Thinking,
    Speaking,
    Complete,
    Error,
};

struct AssistantVoiceSnapshot {
    AssistantVoicePhase phase = AssistantVoicePhase::Idle;
    uint32_t generation = 0;
    uint32_t recorded_ms = 0;
    float level = 0.0f;
    bool retry_available = false;
    std::string message;
    std::string transcript;
    std::string response;
    std::string error;
};

// A2 coordina un turno de voz real reutilizando el dictado existente. Todavía
// no decide una respuesta mediante IA: reproduce una confirmación local y deja
// explícito en el snapshot qué dijo el usuario y qué contestará el firmware.
class AssistantVoiceService {
public:
    AssistantVoiceService(AudioService& audio, AudioSessionManager& audio_sessions,
                          DictationService& dictation);
    bool init();
    AssistantVoiceSnapshot snapshot() const;

    bool start();
    void stop();
    void cancel();
    bool retry();
    void poll();
    bool busy() const;

private:
    enum class Command : uint8_t { PlayAcknowledgement };
    static void task_entry(void* arg);
    void run();
    void set_phase(AssistantVoicePhase phase, const std::string& message = {},
                   const std::string& error = {});
    void update_live(const DictationSnapshot& dictation);

    AudioService& audio_;
    AudioSessionManager& audio_sessions_;
    DictationService& dictation_;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    QueueHandle_t commands_ = nullptr;
    TaskHandle_t task_ = nullptr;
    AssistantVoiceSnapshot snapshot_;
    std::atomic<bool> active_ {false};
    std::atomic<bool> cancel_requested_ {false};
    DictationPhase observed_dictation_phase_ = DictationPhase::Idle;
};

const char* assistant_voice_phase_name(AssistantVoicePhase phase);

}  // namespace el_sistema
