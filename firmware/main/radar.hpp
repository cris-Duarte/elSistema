#pragma once
#include "radar_model.hpp"
#include "dictation.hpp"
#include "audio_session.hpp"
#include "freertos/task.h"

namespace el_sistema {
class RadarService {
public:
    RadarService(DictationService& network, BleService& ble, AudioService& audio, AudioSessionManager& sessions);
    bool init();
    bool visible(bool on);
    bool pause(bool on);
    bool select(uint32_t id);
    bool filter(RadarFilter filter);
    bool sound(bool on);
    RadarSnapshot snapshot() const;
private:
    enum class Command : uint8_t { Visible, Pause, Select, Filter, Sound };
    struct Message { Command command; uint32_t value; };
    bool send(Command command, uint32_t value);
    static void task_entry(void* arg);
    void run();
    void save_preferences();
    DictationService& network_;
    BleService& ble_;
    AudioService& audio_;
    AudioSessionManager& sessions_;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    QueueHandle_t commands_ = nullptr;
    TaskHandle_t task_ = nullptr;
    RadarModel model_;
    RadarSnapshot snapshot_;
};
} // namespace el_sistema
