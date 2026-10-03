#include "assistant.hpp"

namespace el_sistema {

namespace {

constexpr uint64_t kGreetingDurationMs = 1800;
constexpr uint64_t kAttentiveDurationMs = 2600;
constexpr uint64_t kListeningDurationMs = 3400;
constexpr uint64_t kThinkingDurationMs = 2600;
constexpr uint64_t kSpeakingDurationMs = 3800;
constexpr uint64_t kCelebratingDurationMs = 2600;

}  // namespace

const char* assistant_state_name(AssistantVisualState state)
{
    switch (state) {
        case AssistantVisualState::Dormant: return "EN REPOSO";
        case AssistantVisualState::Greeting: return "INICIANDO";
        case AssistantVisualState::Idle: return "DISPONIBLE";
        case AssistantVisualState::Attentive: return "ATENTO";
        case AssistantVisualState::Listening: return "ESCUCHANDO";
        case AssistantVisualState::Thinking: return "PROCESANDO";
        case AssistantVisualState::Speaking: return "RESPONDIENDO";
        case AssistantVisualState::Celebrating: return "CELEBRANDO";
    }
    return "DESCONOCIDO";
}

const char* assistant_state_message(AssistantVisualState state)
{
    switch (state) {
        case AssistantVisualState::Dormant:
            return "El módulo está cerrado y no utiliza audio ni red.";
        case AssistantVisualState::Greeting:
            return "Preparando la presencia visual del asistente.";
        case AssistantVisualState::Idle:
            return "Toca HABLAR para iniciar un turno de voz.";
        case AssistantVisualState::Attentive:
            return "Interacción táctil detectada. Todavía no se envía ninguna consulta.";
        case AssistantVisualState::Listening:
            return "Los micrófonos están capturando tu voz.";
        case AssistantVisualState::Thinking:
            return "El servidor local está convirtiendo tu voz en texto.";
        case AssistantVisualState::Speaking:
            return "El asistente está reproduciendo su respuesta local.";
        case AssistantVisualState::Celebrating:
            return "Animación local preparada para futuros logros y recompensas.";
    }
    return "";
}

void AssistantService::transition(AssistantVisualState state, uint64_t now_ms, uint64_t duration_ms)
{
    snapshot_.state = state;
    snapshot_.entered_at_ms = now_ms;
    deadline_ms_ = duration_ms == 0 ? 0 : now_ms + duration_ms;
    ++snapshot_.generation;
}

void AssistantService::open(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Greeting, now_ms, kGreetingDurationMs);
}

void AssistantService::close(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Dormant, now_ms);
}

void AssistantService::tap(uint64_t now_ms)
{
    if (snapshot_.state == AssistantVisualState::Dormant) return;
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Attentive, now_ms, kAttentiveDurationMs);
}

void AssistantService::start_visual_demo(uint64_t now_ms)
{
    if (snapshot_.state == AssistantVisualState::Dormant) return;
    snapshot_.demo_running = true;
    transition(AssistantVisualState::Listening, now_ms, kListeningDurationMs);
}

void AssistantService::celebrate(uint64_t now_ms)
{
    if (snapshot_.state == AssistantVisualState::Dormant) return;
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Celebrating, now_ms, kCelebratingDurationMs);
}

void AssistantService::begin_listening(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Listening, now_ms);
}

void AssistantService::begin_thinking(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Thinking, now_ms);
}

void AssistantService::begin_speaking(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Speaking, now_ms);
}

void AssistantService::finish_turn(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Idle, now_ms);
}

void AssistantService::fail_turn(uint64_t now_ms)
{
    snapshot_.demo_running = false;
    transition(AssistantVisualState::Attentive, now_ms);
}

bool AssistantService::update(uint64_t now_ms)
{
    if (deadline_ms_ == 0 || now_ms < deadline_ms_) return false;
    switch (snapshot_.state) {
        case AssistantVisualState::Greeting:
        case AssistantVisualState::Attentive:
        case AssistantVisualState::Celebrating:
            snapshot_.demo_running = false;
            transition(AssistantVisualState::Idle, now_ms);
            return true;
        case AssistantVisualState::Listening:
            transition(AssistantVisualState::Thinking, now_ms, kThinkingDurationMs);
            return true;
        case AssistantVisualState::Thinking:
            transition(AssistantVisualState::Speaking, now_ms, kSpeakingDurationMs);
            return true;
        case AssistantVisualState::Speaking:
            snapshot_.demo_running = false;
            transition(AssistantVisualState::Idle, now_ms);
            return true;
        case AssistantVisualState::Dormant:
        case AssistantVisualState::Idle:
            deadline_ms_ = 0;
            return false;
    }
    return false;
}

}  // namespace el_sistema
