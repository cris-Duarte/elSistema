#include "hourglass.hpp"
#include "hardware.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace el_sistema {
bool HourglassService::init()
{
    mutex_ = xSemaphoreCreateMutex();
    commands_ = xQueueCreate(16, sizeof(Command));
    if (!mutex_ || !commands_) {
        if (mutex_) vSemaphoreDelete(mutex_);
        if (commands_) vQueueDelete(commands_);
        mutex_ = nullptr; commands_ = nullptr;
        published_.error = HourglassError::Unavailable;
        return false;
    }
    if (xTaskCreate(task_entry, "hourglass", 8192, this, 2, &task_) != pdPASS) {
        vSemaphoreDelete(mutex_); vQueueDelete(commands_);
        mutex_ = nullptr; commands_ = nullptr;
        published_.error = HourglassError::Unavailable;
        return false;
    }
    return true;
}
uint32_t HourglassService::send(Operation operation, uint16_t minutes)
{
    if (!commands_) return 0;
    const Command command{operation, minutes, next_command_.fetch_add(1)};
    return xQueueSend(commands_, &command, 0) == pdTRUE ? command.id : 0;
}
uint32_t HourglassService::start(uint16_t minutes) { return send(Operation::Start, minutes); }
bool HourglassService::pause() { return send(Operation::Pause); }
bool HourglassService::resume() { return send(Operation::Resume); }
bool HourglassService::reset() { return send(Operation::Reset); }
bool HourglassService::set_visible(bool visible) { return send(visible ? Operation::Visible : Operation::Hidden); }
HourglassSnapshot HourglassService::snapshot()
{
    if (!mutex_) return published_;
    HourglassSnapshot result;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        result = published_;
        xSemaphoreGive(mutex_);
    } else result.error = HourglassError::Unavailable;
    return result;
}
void HourglassService::publish()
{
    HourglassSnapshot state = engine_.snapshot();
    state.command_id = processed_command_;
    if (storage_failed_ && state.error == HourglassError::None) state.error = HourglassError::Storage;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        published_ = state;
        xSemaphoreGive(mutex_);
    }
}
void HourglassService::task_entry(void* data) { static_cast<HourglassService*>(data)->run(); }
void HourglassService::run()
{
    storage_ready_ = repository_.init();
    storage_failed_ = !storage_ready_;
    HourglassSnapshot checkpoint;
    bool corrupt = false;
    if (storage_ready_ && repository_.load(checkpoint, corrupt)) engine_.restore(checkpoint, esp_timer_get_time());
    publish();
    int64_t last_checkpoint_us = esp_timer_get_time();
    uint32_t saved_generation = engine_.snapshot().generation;
    bool dirty = false;
    while (true) {
        Command command{};
        const bool received = xQueueReceive(commands_, &command, pdMS_TO_TICKS(visible_ ? 20 : 100)) == pdTRUE;
        const int64_t now = esp_timer_get_time();
        const MotionSample motion = orientation_.motion_snapshot(); // Fixed panel axes: NO autorotation transform.
        if (visible_) engine_.step(now, motion);
        if (received) {
            switch (command.operation) {
                case Operation::Start: if (visible_) engine_.start(command.minutes, motion, now); break;
                case Operation::Pause: engine_.pause(now); break;
                case Operation::Resume: if (visible_) engine_.resume(motion, now); break;
                case Operation::Reset: engine_.reset(); break;
                case Operation::Visible: visible_ = true; break;
                case Operation::Hidden: engine_.pause(now); visible_ = false; break;
            }
            processed_command_ = command.id;
            dirty = true;
        }
        const HourglassSnapshot& state = engine_.snapshot();
        if (state.generation != saved_generation) dirty = true;
        const bool running_checkpoint = state.state == HourglassState::Running && now - last_checkpoint_us >= 30000000;
        if ((dirty && (!storage_failed_ || now - last_checkpoint_us >= 5000000)) || running_checkpoint) {
            if (!storage_ready_) storage_ready_ = repository_.init();
            const bool saved = storage_ready_ && repository_.save(state);
            storage_failed_ = !saved;
            if (!saved) ESP_LOGW("hourglass", "No se pudo guardar el checkpoint; se reintentará");
            else { dirty = false; saved_generation = state.generation; }
            last_checkpoint_us = now;
        }
        publish();
    }
}
} // namespace el_sistema
