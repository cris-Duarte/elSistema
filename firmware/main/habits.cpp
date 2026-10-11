#include "habits.hpp"
#include "bsp/m5stack_tab5.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <new>

extern const uint8_t habits_voice_start[] asm("_binary_habits_notice_pcm_start");
extern const uint8_t habits_voice_end[] asm("_binary_habits_notice_pcm_end");
namespace el_sistema {
HabitService::HabitService(AudioService &a, AudioSessionManager &s)
    : repository_(BSP_SPIFFS_MOUNT_POINT), audio_(a), sessions_(s) {}
bool HabitService::init() {
    commands_ = xQueueCreate(12, sizeof(HabitCommand *));
    sounds_ = xQueueCreate(1, sizeof(Sound));
    mutex_ = xSemaphoreCreateMutex();
    if (!commands_ || !sounds_ || !mutex_)
        return false;
    return xTaskCreate(task_entry, "habits", 10240, this, 3, &worker_) == pdPASS &&
           xTaskCreate(audio_entry, "habit_audio", 12288, this, 2, &audio_worker_) == pdPASS;
}
void HabitService::clock_context(HabitContext c) {
    taskENTER_CRITICAL(&context_mutex_);
    clock_ = c;
    taskEXIT_CRITICAL(&context_mutex_);
}
HabitContext HabitService::context() const {
    taskENTER_CRITICAL(&context_mutex_);
    const auto c = clock_;
    taskEXIT_CRITICAL(&context_mutex_);
    return c;
}
bool HabitService::post(HabitCommand command) {
    if (!commands_ || !worker_)
        return false;
    auto *p = new (std::nothrow) HabitCommand(std::move(command));
    if (!p)
        return false;
    if (xQueueSend(commands_, &p, 0) != pdTRUE) {
        delete p;
        return false;
    }
    return true;
}
bool HabitService::snapshot(HabitSnapshot &out) {
    if (!mutex_ || xSemaphoreTake(mutex_, 0) != pdTRUE)
        return false;
    out = published_;
    xSemaphoreGive(mutex_);
    return out.available;
}
bool HabitService::audio_can_notify() const {
    const auto owner = sessions_.owner();
    return audio_allowed_.load() && (owner == AudioSessionOwner::None || owner == AudioSessionOwner::Tetris);
}
bool HabitService::can_present() const {
    const auto owner = sessions_.owner();
    return owner == AudioSessionOwner::None || owner == AudioSessionOwner::Tetris ||
           owner == AudioSessionOwner::HabitNotice;
}
void HabitService::cancel_audio() {
    sessions_.cancel_habit_notice();
    if (sounds_)
        xQueueReset(sounds_);
}
void HabitService::task_entry(void *d) {
    static_cast<HabitService *>(d)->run();
}
void HabitService::audio_entry(void *d) {
    static_cast<HabitService *>(d)->audio_run();
}
void HabitService::publish() {
    HabitSnapshot next;
    next.available = true;
    next.document = document_;
    next.generation = ++generation_;
    next.result_sequence = result_sequence_;
    next.result_ok = result_ok_;
    next.result_id = result_id_;
    next.revision = revision_;
    next.presentation_day = presentation_day_;
    next.presentation_ids = presentation_ids_;
    next.notice_sequence = notice_sequence_;
    next.presentation_mode = presentation_mode_;
    next.read_only = read_only_;
    next.error = error_;
    next.storage_warning = repository_.warning();
    next.result_error = result_error_;
    next.pending = pending_habits(document_, context(), false);
    next.unannounced = pending_habits(document_, context(), true);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    published_ = std::move(next);
    xSemaphoreGive(mutex_);
}
void HabitService::run() {
    read_only_ = !repository_.load(document_, error_);
    if (read_only_)
        ESP_LOGE("habits", "Almacenamiento protegido: %s", error_.c_str());
    else
        ESP_LOGI("habits", "Almacenamiento listo: %u hábitos", static_cast<unsigned>(document_.habits.size()));
    publish();
    int64_t last_second = -1;
    for (;;) {
        HabitCommand *pointer = nullptr;
        if (xQueueReceive(commands_, &pointer, pdMS_TO_TICKS(100)) == pdTRUE) {
            std::unique_ptr<HabitCommand> command(pointer);
            command->context = context();
            if (command->operation == HabitOperation::AudioError) {
                error_ = "No se pudo reproducir el aviso. El recordatorio visual sigue disponible.";
                publish();
                continue;
            }
            const bool mutation = command->operation != HabitOperation::Announce && command->sequence != 0;
            if (mutation) {
                result_sequence_ = command->sequence;
                result_id_ = command->id;
                result_ok_ = false;
            }
            if (command->operation == HabitOperation::TestAudio) {
                if (audio_can_notify()) {
                    Sound sound{static_cast<HabitNoticeMode>(std::clamp(command->value, 0, 2)), document_.volume,
                                esp_timer_get_time() + 2000000};
                    const bool accepted = xQueueSend(sounds_, &sound, 0) == pdTRUE;
                    if (mutation)
                        result_ok_ = accepted;
                    error_ = accepted ? "" : "Audio ocupado. No se acumulan pruebas de sonido.";
                } else
                    error_ = "Audio ocupado. Intenta la prueba cuando termine la otra actividad.";
                if (mutation)
                    result_error_ = result_ok_ ? "" : error_;
                publish();
                continue;
            }
            if (read_only_) {
                error_ = "Almacenamiento de hábitos protegido; no se confirmó el cambio.";
                if (mutation)
                    result_error_ = error_;
                publish();
                continue;
            }
            HabitDocument candidate = document_;
            const auto eligible = pending_habits(candidate, command->context, true);
            std::vector<HabitId> announced;
            if (command->operation == HabitOperation::Announce)
                for (auto id : command->ids)
                    if (std::find(eligible.begin(), eligible.end(), id) != eligible.end())
                        announced.push_back(id);
            HabitId result = command->id;
            if (command->operation == HabitOperation::Announce && announced.empty()) {
                publish();
                continue;
            }
            if (apply_habit_command(candidate, *command, result, error_) && repository_.save(candidate, error_)) {
                document_ = std::move(candidate);
                ++revision_;
                if (mutation) {
                    result_ok_ = true;
                    result_id_ = result;
                }
                if (!announced.empty()) {
                    notice_sequence_ = command->sequence;
                    presentation_ids_ = announced;
                    presentation_day_ = civil_day(command->context.utc, command->context.offset_minutes);
                    presentation_mode_ = notice_mode_for(document_, announced);
                    // Sound is requested separately by the UI after the persisted
                    // receipt, so a newly critical activity never gets interrupted.
                }
            }
            if (mutation)
                result_error_ = result_ok_ ? "" : error_;
            publish();
        }
        const int64_t second = context().utc;
        if (second != last_second) {
            last_second = second;
            publish();
        }
    }
}
void HabitService::audio_run() {
    Sound sound;
    for (;;) {
        if (xQueueReceive(sounds_, &sound, portMAX_DELAY) != pdTRUE)
            continue;
        if (sound.mode == HabitNoticeMode::Visual || !sound.volume || esp_timer_get_time() > sound.deadline_us ||
            !audio_can_notify())
            continue;
        AudioSessionLease lease(sessions_, AudioSessionOwner::HabitNotice);
        if (!lease.acquired())
            continue;
        if (esp_timer_get_time() > sound.deadline_us || !audio_allowed_.load())
            continue;
        const auto &cancel = sessions_.habit_notice_cancel_requested();
        const size_t count =
            sound.mode == HabitNoticeMode::Voice ? (habits_voice_end - habits_voice_start) / 2 : 4800;
        struct FreePcm {
            void operator()(int16_t *p) const { heap_caps_free(p); }
        };
        std::unique_ptr<int16_t, FreePcm> pcm(static_cast<int16_t *>(
            heap_caps_malloc(count * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        if (!pcm || !count) {
            HabitCommand fail;
            fail.operation = HabitOperation::AudioError;
            post(fail);
            continue;
        }
        if (sound.mode == HabitNoticeMode::Voice) {
            for (size_t i = 0; i < count; ++i) {
                const int16_t value =
                    static_cast<int16_t>(habits_voice_start[i * 2] | (habits_voice_start[i * 2 + 1] << 8));
                pcm.get()[i] = static_cast<int16_t>(static_cast<int32_t>(value) * sound.volume / 100);
            }
        } else {
            for (size_t i = 0; i < count; ++i) {
                const float envelope = std::min(1.f, static_cast<float>(i) / 160.f) *
                                       std::min(1.f, static_cast<float>(count - i) / 400.f);
                const float frequency = i < 2400 ? 660.f : 880.f;
                pcm.get()[i] = static_cast<int16_t>(8000.f * sound.volume / 100.f * envelope *
                                                    std::sin(6.2831853f * frequency * i / 16000.f));
            }
        }
        // Audio failures are deliberately not allowed to invalidate a saved notice.
        const bool played = audio_.play_pcm(pcm.get(), count, 16000, &cancel);
        ESP_LOGI("habit_audio", "Prueba/aviso: modo=%d, resultado=%s, pila libre mínima=%u bytes",
                 static_cast<int>(sound.mode), played ? "OK" : cancel.load() ? "cancelado" : "error",
                 static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        if (!played && !cancel.load()) {
            HabitCommand fail;
            fail.operation = HabitOperation::AudioError;
            post(fail);
        }
    }
}
} // namespace el_sistema
