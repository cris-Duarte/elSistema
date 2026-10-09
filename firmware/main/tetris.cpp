#include "tetris.hpp"
#include "tetris_sound.hpp"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <algorithm>
namespace el_sistema {
bool TetrisService::init() {
    mutex_=xSemaphoreCreateMutex();commands_=xQueueCreate(24,sizeof(Command));sounds_=xQueueCreate(8,sizeof(Sound));
    if(!mutex_||!commands_||!sounds_||xTaskCreate(task_entry,"tetris",6144,this,2,&worker_)!=pdPASS) {
        if(mutex_) vSemaphoreDelete(mutex_);
        if(commands_) vQueueDelete(commands_);
        if(sounds_) vQueueDelete(sounds_);
        mutex_=nullptr;commands_=sounds_=nullptr;published_.available=false;return false;
    }
    // Audio failure must not disable the game.
    if(xTaskCreate(audio_entry,"tetris-audio",8192,this,2,&audio_worker_)!=pdPASS) ESP_LOGW("tetris","Sin worker de audio");
    return true;
}
bool TetrisService::send(Command c) {return commands_&&xQueueSend(commands_,&c,0)==pdTRUE;}
bool TetrisService::start() {return send({Operation::Start});}
bool TetrisService::resume() {return send({Operation::Resume});}
bool TetrisService::move(int n) {return send({Operation::Move,n});}
bool TetrisService::rotate() {return send({Operation::Rotate});}
bool TetrisService::soft_drop(bool v) {
    soft_desired_.store(v);return true;
}
bool TetrisService::settings(bool music,bool effects,uint8_t volume) {return send({Operation::Settings,volume,music,effects});}
void TetrisService::pause() {soft_desired_.store(false);pause_requested_.store(true);sessions_.cancel_tetris();}
void TetrisService::set_visible(bool v) {visible_.store(v);if(!v) pause();}
TetrisSnapshot TetrisService::snapshot() {
    TetrisSnapshot s{};s.available=false;if(!mutex_) return published_;
    if(xSemaphoreTake(mutex_,0)==pdTRUE) {s=published_;xSemaphoreGive(mutex_);}return s;
}
void TetrisService::publish(bool error,bool corrupt) {
    auto s=engine_.snapshot();s.storage_error=error;s.corrupt=corrupt;
    if(xSemaphoreTake(mutex_,pdMS_TO_TICKS(5))==pdTRUE) {published_=s;xSemaphoreGive(mutex_);}
}
void TetrisService::task_entry(void* p) {static_cast<TetrisService*>(p)->run();}
void TetrisService::audio_entry(void* p) {static_cast<TetrisService*>(p)->audio_run();}
void TetrisService::run() {
    bool ready=repository_.init(),corrupt=false,error=!ready,dirty=false;
    TetrisSnapshot recovered;
    if(ready&&repository_.load(recovered,corrupt)) engine_.restore(recovered);
    int64_t last=esp_timer_get_time(),checkpoint=last,debt_us=0;publish(error,corrupt);
    while(true) {
        Command c{};const bool received=xQueueReceive(commands_,&c,pdMS_TO_TICKS(10))==pdTRUE;
        const int64_t now=esp_timer_get_time();const auto before=engine_.snapshot().state;
        if(pause_requested_.exchange(false)||!visible_.load()) {
            debt_us=0;
            if(engine_.snapshot().state==TetrisState::Running) {engine_.pause();dirty=true;}
            // Never retain touch commands across pause/navigation. Settings can
            // be submitted again from the stable paused menu.
            if(before==TetrisState::Running) {xQueueReset(commands_);xQueueReset(sounds_);}
        } else {
            engine_.set_soft_drop(soft_desired_.load());
            if(engine_.snapshot().state==TetrisState::Running) {
                debt_us+=now-last;
                // Preserve sub-millisecond remainder and scheduler delays;
                // bounded work per iteration prevents starving UI/other tasks.
                for(int steps=0;debt_us>=1000&&steps<50;++steps) {
                    const auto delta=uint32_t(std::min<int64_t>(debt_us/1000,20));
                    engine_.advance(delta);debt_us-=delta*1000;
                }
            } else debt_us=0;
        }
        last=now;
        if(received) {
            const bool interrupted=before==TetrisState::Running&&engine_.snapshot().state==TetrisState::Paused;
            if(!interrupted) switch(c.operation) {
                case Operation::Start: if(visible_) {engine_.start(esp_random());dirty=true;corrupt=false;} break;
                case Operation::Resume: if(visible_) engine_.resume();break;
                case Operation::Move: engine_.move(c.value);break;
                case Operation::Rotate: engine_.rotate();break;
                case Operation::Settings:engine_.settings(c.music,c.effects,c.value);dirty=true;break;
            }
        }
        if(engine_.snapshot().state!=before) dirty=true;
        const uint32_t events=engine_.take_sounds();
        if(events) {const Sound sound{events,now};xQueueSend(sounds_,&sound,0);}
        // Confirm pause/state changes to the UI and audio before flash I/O.
        // A slow NVS commit must not delay a reminder or the pause menu.
        publish(error,corrupt);
        if((dirty&&(!error||now-checkpoint>=5000000))||
           (engine_.snapshot().state==TetrisState::Running&&now-checkpoint>=30000000)) {
            if(!ready) ready=repository_.init();
            error=!ready||!repository_.save(engine_.snapshot());
            if(!error) dirty=false;
            checkpoint=now;
        }
        publish(error,corrupt);
    }
}
void TetrisService::audio_run() {
    TetrisSynth synth;int16_t pcm[320];bool previously_running=false;
    int64_t retry_at=0,tail_until=0;
    while(true) {
        const auto state=snapshot();const int64_t now=esp_timer_get_time();
        const bool running=visible_.load()&&state.available&&state.state==TetrisState::Running;
        Sound sound{};uint32_t events=0;
        while(xQueueReceive(sounds_,&sound,0)==pdTRUE) if(now-sound.timestamp<100000) events|=sound.events;
        if(!running&&!(state.state==TetrisState::Finished&&visible_&&
                     ((events&TetrisEnd)||now<tail_until))) {
            tail_until=0;synth.reset();previously_running=false;
            vTaskDelay(pdMS_TO_TICKS(20));continue;
        }
        if(running&&!previously_running) synth.reset();
        previously_running=running;
        if(state.effects&&events) {synth.trigger(events);tail_until=now+320000;}
        if(!state.volume||(!state.music&&(!state.effects||now>=tail_until))||now<retry_at) {
            vTaskDelay(pdMS_TO_TICKS(20));continue;
        }
        AudioSessionLease lease(sessions_,AudioSessionOwner::Tetris);
        if(!lease.acquired()) {synth.reset();vTaskDelay(pdMS_TO_TICKS(20));continue;}
        synth.render(pcm,320,running&&state.music,state.volume);
        if(!audio_.play_pcm(pcm,320,16000,&sessions_.tetris_cancel_requested())) {
            synth.reset();retry_at=now+1000000;
        }
    }
}
}
