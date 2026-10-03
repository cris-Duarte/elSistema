#include "assistant_voice.hpp"

#include "esp_log.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "assistant_voice";
constexpr const char* kAcknowledgement = "Te escucho. El canal de voz está listo.";

extern const uint8_t assistant_ack_pcm_start[] asm("_binary_assistant_ack_pcm_start");
extern const uint8_t assistant_ack_pcm_end[] asm("_binary_assistant_ack_pcm_end");

}  // namespace

const char* assistant_voice_phase_name(AssistantVoicePhase phase)
{
    switch (phase) {
        case AssistantVoicePhase::Idle: return "DISPONIBLE";
        case AssistantVoicePhase::Listening: return "ESCUCHANDO";
        case AssistantVoicePhase::Thinking: return "TRANSCRIBIENDO";
        case AssistantVoicePhase::Speaking: return "RESPONDIENDO";
        case AssistantVoicePhase::Complete: return "TURNO COMPLETADO";
        case AssistantVoicePhase::Error: return "ERROR DE VOZ";
    }
    return "ERROR";
}

AssistantVoiceService::AssistantVoiceService(AudioService& audio, AudioSessionManager& audio_sessions,
                                             DictationService& dictation)
    : audio_(audio), audio_sessions_(audio_sessions), dictation_(dictation)
{
}

bool AssistantVoiceService::init()
{
    mutex_ = xSemaphoreCreateMutex();
    commands_ = xQueueCreate(2, sizeof(Command));
    if (mutex_ == nullptr || commands_ == nullptr) return false;
    snapshot_.message = "Toca HABLAR para iniciar un turno de voz local.";
    return xTaskCreate(task_entry, "assistant_voice", 6144, this, 5, &task_) == pdPASS;
}

AssistantVoiceSnapshot AssistantVoiceService::snapshot() const
{
    AssistantVoiceSnapshot value;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(30)) == pdTRUE) {
        value = snapshot_;
        xSemaphoreGive(mutex_);
    }
    return value;
}

void AssistantVoiceService::set_phase(AssistantVoicePhase phase, const std::string& message,
                                      const std::string& error)
{
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    snapshot_.phase = phase;
    snapshot_.message = message;
    snapshot_.error = error;
    ++snapshot_.generation;
    xSemaphoreGive(mutex_);
}

bool AssistantVoiceService::start()
{
    if (busy() || dictation_.busy()) return false;
    cancel_requested_.store(false);
    active_.store(true);
    observed_dictation_phase_ = dictation_.snapshot().phase;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_ = {};
        snapshot_.phase = AssistantVoicePhase::Listening;
        snapshot_.message = "Preparando los micrófonos…";
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
    dictation_.start(AudioSessionOwner::AssistantCapture);
    return true;
}

void AssistantVoiceService::stop()
{
    if (active_.load()) dictation_.stop();
}

void AssistantVoiceService::cancel()
{
    cancel_requested_.store(true);
    dictation_.cancel();
    dictation_.discard();
    active_.store(false);
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        const uint32_t generation = snapshot_.generation + 1;
        snapshot_ = {};
        snapshot_.generation = generation;
        snapshot_.message = "Turno cancelado. Toca HABLAR para comenzar de nuevo.";
        xSemaphoreGive(mutex_);
    }
}

bool AssistantVoiceService::retry()
{
    const AssistantVoiceSnapshot current = snapshot();
    if (busy() || !current.retry_available || !dictation_.has_audio()) return false;
    cancel_requested_.store(false);
    active_.store(true);
    observed_dictation_phase_ = dictation_.snapshot().phase;
    set_phase(AssistantVoicePhase::Thinking, "Buscando nuevamente el MacBook…");
    dictation_.retry();
    return true;
}

bool AssistantVoiceService::busy() const
{
    const AssistantVoicePhase phase = snapshot().phase;
    return phase == AssistantVoicePhase::Listening || phase == AssistantVoicePhase::Thinking ||
           phase == AssistantVoicePhase::Speaking;
}

void AssistantVoiceService::update_live(const DictationSnapshot& dictation)
{
    if (mutex_ == nullptr || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    snapshot_.recorded_ms = dictation.recorded_ms;
    snapshot_.level = dictation.level;
    xSemaphoreGive(mutex_);
}

void AssistantVoiceService::poll()
{
    if (!active_.load()) return;
    const DictationSnapshot dictation = dictation_.snapshot();
    update_live(dictation);
    if (dictation.phase == observed_dictation_phase_) return;
    observed_dictation_phase_ = dictation.phase;

    switch (dictation.phase) {
        case DictationPhase::Preparing:
        case DictationPhase::Recording:
            set_phase(AssistantVoicePhase::Listening,
                      dictation.phase == DictationPhase::Recording
                          ? "Habla ahora. Toca DETENER cuando termines."
                          : "Preparando los micrófonos…");
            break;
        case DictationPhase::Processing:
        case DictationPhase::Connecting:
        case DictationPhase::Uploading:
        case DictationPhase::Transcribing:
            set_phase(AssistantVoicePhase::Thinking,
                      dictation.message.empty() ? "Procesando tu voz…" : dictation.message);
            break;
        case DictationPhase::Result: {
            const std::string transcript = dictation_.accept_result();
            if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                snapshot_.phase = AssistantVoicePhase::Speaking;
                snapshot_.transcript = transcript;
                snapshot_.response = kAcknowledgement;
                snapshot_.message = "Respuesta local de validación";
                snapshot_.retry_available = false;
                snapshot_.error.clear();
                ++snapshot_.generation;
                xSemaphoreGive(mutex_);
            }
            const Command command = Command::PlayAcknowledgement;
            if (xQueueSend(commands_, &command, 0) != pdTRUE) {
                active_.store(false);
                set_phase(AssistantVoicePhase::Error, {}, "No se pudo iniciar la respuesta hablada");
            }
            break;
        }
        case DictationPhase::Error:
        case DictationPhase::LocalResult:
            active_.store(false);
            if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                snapshot_.phase = AssistantVoicePhase::Error;
                snapshot_.message = dictation.message;
                snapshot_.error = dictation.error.empty() ? "No fue posible completar el turno" : dictation.error;
                snapshot_.retry_available = dictation.has_audio;
                ++snapshot_.generation;
                xSemaphoreGive(mutex_);
            }
            break;
        case DictationPhase::Unconfigured:
            active_.store(false);
            set_phase(AssistantVoicePhase::Error, {}, "Configura la red y selecciona tu MacBook");
            break;
        case DictationPhase::Idle:
            break;
    }
}

void AssistantVoiceService::task_entry(void* arg)
{
    static_cast<AssistantVoiceService*>(arg)->run();
}

void AssistantVoiceService::run()
{
    while (true) {
        Command command;
        if (xQueueReceive(commands_, &command, portMAX_DELAY) != pdTRUE) continue;
        if (command != Command::PlayAcknowledgement) continue;

        AudioSessionLease session(audio_sessions_, AudioSessionOwner::AssistantPlayback);
        if (!session.acquired()) {
            active_.store(false);
            set_phase(AssistantVoicePhase::Error, {},
                      std::string("El audio está ocupado por ") +
                          audio_session_owner_name(audio_sessions_.owner()));
            continue;
        }
        const size_t bytes = static_cast<size_t>(assistant_ack_pcm_end - assistant_ack_pcm_start);
        const bool played = audio_.play_pcm(reinterpret_cast<const int16_t*>(assistant_ack_pcm_start),
                                            bytes / sizeof(int16_t), 16000, &cancel_requested_);
        session.release();
        if (cancel_requested_.load()) continue;
        active_.store(false);
        if (played) {
            set_phase(AssistantVoicePhase::Complete,
                      "La captura, transcripción y reproducción finalizaron correctamente.");
            ESP_LOGI(TAG, "Turno A2 completado");
        } else {
            set_phase(AssistantVoicePhase::Error, {}, audio_.last_error());
        }
    }
}

}  // namespace el_sistema
