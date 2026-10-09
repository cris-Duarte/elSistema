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
        case AudioSessionOwner::Radar: return "pitido del radar";
        case AudioSessionOwner::Tetris: return "música y efectos de Tetris";
    }
    return "desconocido";
}

bool AudioSessionManager::try_acquire(AudioSessionOwner owner)
{
    if (owner == AudioSessionOwner::None) return false;
    if ((owner == AudioSessionOwner::Radar || owner == AudioSessionOwner::Tetris) &&
        priority_waiters_.load() != 0) return false;
    if (owner == AudioSessionOwner::Radar) radar_cancel_.store(false);
    if (owner == AudioSessionOwner::Tetris) tetris_cancel_.store(false);
    AudioSessionOwner expected = AudioSessionOwner::None;
    const bool acquired = owner_.compare_exchange_strong(expected, owner);
    if (!acquired && owner != AudioSessionOwner::Radar && owner != AudioSessionOwner::Tetris) {
        if (expected == AudioSessionOwner::Radar) cancel_radar();
        if (expected == AudioSessionOwner::Tetris) cancel_tetris();
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
    const bool priority = owner != AudioSessionOwner::None && owner != AudioSessionOwner::Radar &&
                          owner != AudioSessionOwner::Tetris;
    if (priority) manager_.priority_waiters_.fetch_add(1);
    acquired_ = manager_.try_acquire(owner);
    // Radio beeps and game audio are preemptible. Other ownership rules stay
    // non-blocking. This wait occurs on audio workers, never on the LVGL task.
    if (!acquired_ && owner != AudioSessionOwner::Radar && owner != AudioSessionOwner::Tetris &&
        (manager_.owner() == AudioSessionOwner::Radar || manager_.owner() == AudioSessionOwner::Tetris ||
         manager_.owner() == AudioSessionOwner::None)) {
        const TickType_t start = xTaskGetTickCount();
        // First codec initialization or a failing I2S write may take longer
        // than the 35 ms pulse. Waiting here cannot freeze the UI.
        const TickType_t budget = pdMS_TO_TICKS(1200);
        while (!acquired_ && xTaskGetTickCount()-start < budget) {
            vTaskDelay(1);
            acquired_ = manager_.try_acquire(owner_);
            if (!acquired_ && manager_.owner() != AudioSessionOwner::Radar && manager_.owner() != AudioSessionOwner::Tetris &&
                manager_.owner() != AudioSessionOwner::None) break;
        }
    }
    if (priority) manager_.priority_waiters_.fetch_sub(1);
}

AudioSessionLease::~AudioSessionLease() { release(); }

void AudioSessionLease::release()
{
    if (!acquired_) return;
    manager_.release(owner_);
    acquired_ = false;
}

}  // namespace el_sistema
