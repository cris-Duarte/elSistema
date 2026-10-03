#include "audio_session.hpp"

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
    }
    return "desconocido";
}

bool AudioSessionManager::try_acquire(AudioSessionOwner owner)
{
    if (owner == AudioSessionOwner::None) return false;
    AudioSessionOwner expected = AudioSessionOwner::None;
    return owner_.compare_exchange_strong(expected, owner);
}

void AudioSessionManager::release(AudioSessionOwner owner)
{
    if (owner == AudioSessionOwner::None) return;
    AudioSessionOwner expected = owner;
    owner_.compare_exchange_strong(expected, AudioSessionOwner::None);
}

AudioSessionLease::AudioSessionLease(AudioSessionManager& manager, AudioSessionOwner owner)
    : manager_(manager), owner_(owner), acquired_(manager.try_acquire(owner))
{
}

AudioSessionLease::~AudioSessionLease() { release(); }

void AudioSessionLease::release()
{
    if (!acquired_) return;
    manager_.release(owner_);
    acquired_ = false;
}

}  // namespace el_sistema
