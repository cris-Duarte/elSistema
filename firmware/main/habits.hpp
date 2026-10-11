#pragma once
#include "audio.hpp"
#include "audio_session.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "habit_repository.hpp"
#include <atomic>
namespace el_sistema {
class HabitService {
  public:
    HabitService(AudioService &audio, AudioSessionManager &sessions);
    bool init();
    void clock_context(HabitContext context);
    // Ownership is transferred to the worker only if the bounded queue accepts.
    bool post(HabitCommand command);
    bool snapshot(HabitSnapshot &destination);
    bool audio_can_notify() const;
    bool can_present() const;
    void allow_audio(bool allowed) {
        audio_allowed_.store(allowed);
        if (!allowed)
            cancel_audio();
    }
    void cancel_audio();

  private:
    struct Sound {
        HabitNoticeMode mode;
        uint8_t volume;
        int64_t deadline_us;
    };
    static void task_entry(void *data);
    static void audio_entry(void *data);
    HabitContext context() const;
    void run();
    void audio_run();
    void publish();
    HabitRepository repository_;
    HabitDocument document_;
    HabitSnapshot published_;
    AudioService &audio_;
    AudioSessionManager &sessions_;
    QueueHandle_t commands_ = nullptr, sounds_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t worker_ = nullptr, audio_worker_ = nullptr;
    HabitContext clock_{};
    mutable portMUX_TYPE context_mutex_ = portMUX_INITIALIZER_UNLOCKED;
    bool read_only_ = false;
    uint32_t generation_ = 0, revision_ = 1, result_sequence_ = 0, notice_sequence_ = 0;
    CivilDay presentation_day_ = 0;
    std::vector<HabitId> presentation_ids_;
    HabitId result_id_ = 0;
    bool result_ok_ = false;
    std::string error_;
    std::string result_error_;
    HabitNoticeMode presentation_mode_ = HabitNoticeMode::Visual;
    std::atomic<bool> audio_allowed_{false};
};
} // namespace el_sistema
