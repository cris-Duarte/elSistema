#include "audio_session.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace el_sistema {

const char* audio_session_owner_name(AudioSessionOwner owner)
{
    switch (owner) {
        case AudioSessionOwner::None: return "libre";
        case AudioSessionOwner::DictationCapture: return "captura de dictado";
        case AudioSessionOwner::DictationPlayback: return "reproducción de dictado";
        case AudioSessionOwner::AssistantCapture: return "captura del asistente";
        case AudioSessionOwner::AssistantPlayback: return "voz del asistente";
        case AudioSessionOwner::Reminder: return "recordatorio hablado";
        case AudioSessionOwner::Diagnostic: return "diagnóstico de audio";
        case AudioSessionOwner::Tetris: return "música y efectos de Tetris";
        case AudioSessionOwner::HabitNotice: return "aviso de hábitos";
    }
    return "desconocido";
}

bool AudioSessionManager::try_acquire(AudioSessionOwner owner)
{
    if (owner == AudioSessionOwner::None) return false;
    if (owner == AudioSessionOwner::Tetris &&
        (priority_waiters_.load() != 0 || habit_waiters_.load() != 0)) return false;
    if (owner == AudioSessionOwner::Tetris) tetris_cancel_.store(false);
    if (owner == AudioSessionOwner::HabitNotice && priority_waiters_.load() != 0) return false;
    if (owner == AudioSessionOwner::HabitNotice) habit_cancel_.store(false);
    AudioSessionOwner expected = AudioSessionOwner::None;
    const bool acquired = owner_.compare_exchange_strong(expected, owner);
    if (!acquired && owner != AudioSessionOwner::Tetris) {
        if (expected == AudioSessionOwner::Tetris) cancel_tetris();
        if (expected == AudioSessionOwner::HabitNotice && owner != AudioSessionOwner::HabitNotice)
            cancel_habit_notice();
    }
    return acquired;
}

void AudioSessionManager::release(AudioSessionOwner owner)
{
    if (owner == AudioSessionOwner::None) return;
    AudioSessionOwner expected = owner;
    owner_.compare_exchange_strong(expected, AudioSessionOwner::None);
}

AudioSessionLease::AudioSessionLease(AudioSessionManager& manager, AudioSessionOwner owner)
    : manager_(manager), owner_(owner)
{
    const bool priority = owner != AudioSessionOwner::None &&
                          owner != AudioSessionOwner::Tetris && owner != AudioSessionOwner::HabitNotice;
    if (priority) manager_.priority_waiters_.fetch_add(1);
    if (owner == AudioSessionOwner::HabitNotice) manager_.habit_waiters_.fetch_add(1);
    acquired_ = manager_.try_acquire(owner);
    // Game audio is preemptible. Other ownership rules stay
    // non-blocking. This wait occurs on audio workers, never on the LVGL task.
    if (!acquired_ && owner != AudioSessionOwner::Tetris &&
        (manager_.owner() == AudioSessionOwner::Tetris || manager_.owner() == AudioSessionOwner::HabitNotice ||
         manager_.owner() == AudioSessionOwner::None)) {
        const TickType_t start = xTaskGetTickCount();
        // First codec initialization or a failing I2S write may take longer
        // than a short effect. Waiting here cannot freeze the UI.
        const TickType_t budget = pdMS_TO_TICKS(1200);
        while (!acquired_ && xTaskGetTickCount()-start < budget) {
            vTaskDelay(1);
            acquired_ = manager_.try_acquire(owner_);
            if (!acquired_ && manager_.owner() != AudioSessionOwner::Tetris && manager_.owner() != AudioSessionOwner::HabitNotice &&
                manager_.owner() != AudioSessionOwner::None) break;
        }
    }
    if (priority) manager_.priority_waiters_.fetch_sub(1);
    if (owner == AudioSessionOwner::HabitNotice) manager_.habit_waiters_.fetch_sub(1);
}

AudioSessionLease::~AudioSessionLease() { release(); }

void AudioSessionLease::release()
{
    if (!acquired_) return;
    manager_.release(owner_);
    acquired_ = false;
}

}  // namespace el_sistema
