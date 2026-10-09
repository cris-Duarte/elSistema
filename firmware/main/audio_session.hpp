#pragma once

#include <atomic>
#include <cstdint>

namespace el_sistema {

// Un unico propietario puede usar el codec a la vez. El asistente, los avisos
// hablados y el dictado compartirán este arbitro en lugar de coordinarse entre
// si mediante banderas independientes.
enum class AudioSessionOwner : uint8_t {
    None,
    DictationCapture,
    DictationPlayback,
    AssistantCapture,
    AssistantPlayback,
    Reminder,
    Diagnostic,
    Radar,
    Tetris,
};

const char* audio_session_owner_name(AudioSessionOwner owner);

class AudioSessionManager {
public:
    bool try_acquire(AudioSessionOwner owner);
    void release(AudioSessionOwner owner);
    AudioSessionOwner owner() const { return owner_.load(); }
    bool busy() const { return owner() != AudioSessionOwner::None; }
    void cancel_radar() { radar_cancel_.store(true); }
    const std::atomic<bool>& radar_cancel_requested() const { return radar_cancel_; }
    void cancel_tetris() { tetris_cancel_.store(true); }
    const std::atomic<bool>& tetris_cancel_requested() const { return tetris_cancel_; }

private:
    friend class AudioSessionLease;
    std::atomic<unsigned> priority_waiters_ {0};
    std::atomic<AudioSessionOwner> owner_ {AudioSessionOwner::None};
    std::atomic<bool> radar_cancel_ {false};
    std::atomic<bool> tetris_cancel_ {false};
};

class AudioSessionLease {
public:
    AudioSessionLease(AudioSessionManager& manager, AudioSessionOwner owner);
    ~AudioSessionLease();

    AudioSessionLease(const AudioSessionLease&) = delete;
    AudioSessionLease& operator=(const AudioSessionLease&) = delete;

    bool acquired() const { return acquired_; }
    void release();

private:
    AudioSessionManager& manager_;
    AudioSessionOwner owner_;
    bool acquired_ = false;
};

}  // namespace el_sistema
