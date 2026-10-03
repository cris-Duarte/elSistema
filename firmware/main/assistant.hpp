#pragma once

#include <cstdint>

namespace el_sistema {

// Estados visuales compartidos por la demostración A1 y el turno de voz A2.
// Ninguno implica por sí solo que exista un modelo conversacional.
enum class AssistantVisualState : uint8_t {
    Dormant,
    Greeting,
    Idle,
    Attentive,
    Listening,
    Thinking,
    Speaking,
    Celebrating,
};

struct AssistantSnapshot {
    AssistantVisualState state = AssistantVisualState::Dormant;
    uint32_t generation = 0;
    uint64_t entered_at_ms = 0;
    bool demo_running = false;
};

const char* assistant_state_name(AssistantVisualState state);
const char* assistant_state_message(AssistantVisualState state);

// Máquina de estados determinista y sin dependencias de LVGL. En A2 los
// servicios de voz/IA enviarán eventos a esta misma interfaz; A1 solo ofrece
// un ciclo demostrativo local para validar presencia, transiciones y animación.
class AssistantService {
public:
    void open(uint64_t now_ms);
    void close(uint64_t now_ms);
    void tap(uint64_t now_ms);
    void start_visual_demo(uint64_t now_ms);
    void celebrate(uint64_t now_ms);
    void begin_listening(uint64_t now_ms);
    void begin_thinking(uint64_t now_ms);
    void begin_speaking(uint64_t now_ms);
    void finish_turn(uint64_t now_ms);
    void fail_turn(uint64_t now_ms);
    bool update(uint64_t now_ms);

    AssistantSnapshot snapshot() const { return snapshot_; }

private:
    void transition(AssistantVisualState state, uint64_t now_ms, uint64_t duration_ms = 0);

    AssistantSnapshot snapshot_;
    uint64_t deadline_ms_ = 0;
};

}  // namespace el_sistema
