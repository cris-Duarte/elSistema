#pragma once
#include "tetris_model.hpp"
#include "tetris_repository.hpp"
#include "audio.hpp"
#include "audio_session.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>
namespace el_sistema {
class TetrisService {
public:
    TetrisService(AudioService& audio,AudioSessionManager& sessions):audio_(audio),sessions_(sessions) {}
    bool init();
    bool start(); bool resume(); bool move(int columns); bool rotate(); bool soft_drop(bool enabled);
    bool settings(bool music,bool effects,uint8_t volume);
    void pause();
    void set_visible(bool visible);
    TetrisSnapshot snapshot();
private:
    enum class Operation:uint8_t {Start,Resume,Move,Rotate,Settings};
    struct Command {Operation operation;int value=0;bool music=false,effects=false;};
    struct Sound {uint32_t events;int64_t timestamp;};
    bool send(Command command);
    static void task_entry(void* data); static void audio_entry(void* data);
    void run(); void audio_run(); void publish(bool error,bool corrupt);
    AudioService& audio_;AudioSessionManager& sessions_;
    TetrisEngine engine_;TetrisRepository repository_;TetrisSnapshot published_{};
    QueueHandle_t commands_=nullptr,sounds_=nullptr;
    SemaphoreHandle_t mutex_=nullptr;
    TaskHandle_t worker_=nullptr,audio_worker_=nullptr;
    std::atomic<bool> visible_{false},pause_requested_{false},soft_desired_{false};
};
}
