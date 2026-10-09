#pragma once
#include "maze_model.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>
namespace el_sistema {
class OrientationService;
class MazeService {
public:
    explicit MazeService(OrientationService& orientation):orientation_(orientation) {}
    bool init();
    bool start();
    bool resume();
    bool calibrate();
    bool select(unsigned level);
    void pause() { pause_requested_.store(true); }
    void set_visible(bool visible) {if(visible_.exchange(visible)&&!visible) pause();}
    MazeSnapshot snapshot();
private:
    enum class Operation : uint8_t { Start,Resume,Calibrate,Select };
    struct Command {Operation operation;unsigned level=0;};
    bool send(Command command);
    static void task_entry(void* data);
    void run();
    void publish(bool storage_error);
    OrientationService& orientation_;
    MazeEngine engine_;
    MazeSnapshot published_{};
    QueueHandle_t commands_=nullptr;
    SemaphoreHandle_t mutex_=nullptr;
    TaskHandle_t task_=nullptr;
    std::atomic<bool> visible_{false},pause_requested_{false};
};
}
