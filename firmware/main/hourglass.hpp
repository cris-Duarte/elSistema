#pragma once

#include "hourglass_model.hpp"
#include "hourglass_repository.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>

namespace el_sistema {
class OrientationService;

// Only the worker owns the engine and NVS. The UI sends commands and reads a
// bounded snapshot; neither sensor acquisition nor persistence runs in LVGL.
class HourglassService {
public:
    explicit HourglassService(OrientationService& orientation) : orientation_(orientation) {}
    bool init();
    uint32_t start(uint16_t minutes);
    bool pause();
    bool resume();
    bool reset();
    bool set_visible(bool visible);
    HourglassSnapshot snapshot();

private:
    enum class Operation : uint8_t { Start, Pause, Resume, Reset, Visible, Hidden };
    struct Command { Operation operation; uint16_t minutes = 25; uint32_t id = 0; };
    uint32_t send(Operation operation, uint16_t minutes = 25);
    static void task_entry(void* data);
    void run();
    void publish();
    OrientationService& orientation_;
    HourglassRepository repository_;
    HourglassEngine engine_;
    HourglassSnapshot published_;
    QueueHandle_t commands_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t task_ = nullptr;
    bool visible_ = false;
    bool storage_ready_ = false;
    bool storage_failed_ = false;
    std::atomic<uint32_t> next_command_{1};
    uint32_t processed_command_ = 0;
};
} // namespace el_sistema
