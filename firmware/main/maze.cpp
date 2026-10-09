#include "maze.hpp"
#include "hardware.hpp"
#include "esp_timer.h"
#include "nvs.h"
namespace el_sistema {
bool MazeService::init() {
    mutex_=xSemaphoreCreateMutex();commands_=xQueueCreate(12,sizeof(Command));
    if(mutex_&&commands_&&xTaskCreate(task_entry,"maze",6144,this,2,&task_)==pdPASS) return true;
    if(mutex_) vSemaphoreDelete(mutex_);
    if(commands_) vQueueDelete(commands_);
    mutex_=nullptr;commands_=nullptr;published_.available=false;published_.error=MazeError::Unavailable;return false;
}
bool MazeService::send(Command c) {return commands_&&xQueueSend(commands_,&c,0)==pdTRUE;}
bool MazeService::start() {return send({Operation::Start});}
bool MazeService::resume() {return send({Operation::Resume});}
bool MazeService::calibrate() {return send({Operation::Calibrate});}
bool MazeService::select(unsigned level) {return level<MAZE_LEVELS&&send({Operation::Select,level});}
MazeSnapshot MazeService::snapshot() {
    if(!mutex_) return published_;
    MazeSnapshot s;s.available=false;
    if(xSemaphoreTake(mutex_,0)==pdTRUE) {s=published_;xSemaphoreGive(mutex_);}return s;
}
void MazeService::publish(bool storage_error) {
    auto s=engine_.snapshot();s.storage_error=storage_error;
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(5))==pdTRUE) {published_=s;xSemaphoreGive(mutex_);}
}
void MazeService::task_entry(void* data) {static_cast<MazeService*>(data)->run();}
void MazeService::run() {
    nvs_handle_t store=0;bool ready=nvs_open("maze",NVS_READWRITE,&store)==ESP_OK,error=!ready,dirty=false;
    if(ready) {
        MazePreferences bytes{};size_t size=bytes.size();unsigned level=0;std::array<uint32_t,MAZE_LEVELS> best{};
        const esp_err_t result=nvs_get_blob(store,"preferences",bytes.data(),&size);
        if(result==ESP_OK&&size==bytes.size()&&decode_maze_preferences(bytes,level,best)) engine_.preferences(level,best);
        else if(result!=ESP_ERR_NVS_NOT_FOUND) {error=true;dirty=true;}
    }
    int64_t last_save=esp_timer_get_time();publish(error);
    while(true) {
        Command c{};const bool received=xQueueReceive(commands_,&c,pdMS_TO_TICKS(visible_.load()?20:100))==pdTRUE;
        const int64_t now=esp_timer_get_time();const auto before=engine_.snapshot();
        const bool interrupted=pause_requested_.exchange(false)||!visible_.load();
        const bool was_active=before.state==MazeState::Running||before.state==MazeState::Calibrating;
        if(interrupted) {
            engine_.pause(now);
            if(was_active) xQueueReset(commands_);
        } else {
            const MotionSample motion=orientation_.motion_snapshot();engine_.step(now,motion);
        }
        if(received&&visible_.load()&&!(interrupted&&was_active)) {
            const MotionSample motion=orientation_.motion_snapshot();
            switch(c.operation) {
                case Operation::Start:engine_.start(now,motion);break;
                case Operation::Resume:engine_.resume(now,motion);break;
                case Operation::Calibrate:
                    if(engine_.snapshot().state==MazeState::Paused||engine_.snapshot().state==MazeState::Running)
                        engine_.calibrate(now);
                    break;
                case Operation::Select:engine_.select(c.level);break;
            }
        }
        const auto& s=engine_.snapshot();
        if(s.level!=before.level||s.best_ms!=before.best_ms) dirty=true;
        publish(error); // UI remains responsive even if an NVS commit is slow.
        if(dirty&&(!error||now-last_save>=5000000)) {
            if(!ready) ready=nvs_open("maze",NVS_READWRITE,&store)==ESP_OK;
            const auto bytes=encode_maze_preferences(s);
            error=!ready||nvs_set_blob(store,"preferences",bytes.data(),bytes.size())!=ESP_OK||nvs_commit(store)!=ESP_OK;
            if(!error) dirty=false;
            last_save=now;publish(error);
        }
    }
}
}
