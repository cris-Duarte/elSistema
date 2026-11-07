#include "DisplayBackend.h"
#include "Diagnostics.h"
#include "Game.h"
#include "Hud.h"
#include "Input.h"
#include "Renderer.h"
#ifdef VOXEL_BENCHMARK
#include "Benchmark.h"
#endif
#include "M5Unified.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <algorithm>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace {
voxel::DisplayBackend display;
voxel::Game game;
voxel::Renderer renderer;
voxel::Input input;
voxel::Diagnostics diagnostics;
#ifdef VOXEL_BENCHMARK
voxel::Benchmark benchmark;
void logBenchmarkScene() {
    ESP_LOGI("voxel","VOXEL_BENCH {\"event\":\"scene\",\"scene\":\"%s\",\"warmup_s\":3,\"measure_s\":60}",
             voxel::Benchmark::name(benchmark.scene()));
}
void logBenchmarkResult() {
    const auto& r=benchmark.report();
    ESP_LOGI("voxel","VOXEL_BENCH {\"event\":\"result\",\"scene\":\"%s\",\"elapsed_us\":%llu,\"frames\":%lu,\"samples\":%lu,\"fps\":%.3f,\"min_window_fps\":%.3f,\"render_ms\":%.3f,\"present_ms\":%.3f,\"min_psram\":%u,\"min_internal\":%u,\"fps_ok\":%s}",
        voxel::Benchmark::name(r.scene),static_cast<unsigned long long>(r.elapsedUs),
        static_cast<unsigned long>(r.frames),static_cast<unsigned long>(r.samples),r.fps,r.minWindowFps,
        r.renderMs,r.presentMs,static_cast<unsigned>(r.minPsram),static_cast<unsigned>(r.minInternal),
        r.hasWindow && r.minWindowFps>=20.f?"true":"false");
}
#endif
uint64_t previous=0,lastLog=0;
uint32_t frames=0;
bool ready=false,failed=false;
void fail(const char* message,bool beforePresentation) {
    ESP_LOGE("voxel","%s",message);
    if(beforePresentation) {
        M5.Display.fillScreen(0x7800); M5.Display.setTextColor(0xffff);
        M5.Display.setTextSize(2); M5.Display.setCursor(20,30);
        M5.Display.printf("TAB5 VOXEL\n%s\nReinicia el dispositivo.",message);
    }
    failed=true;
}
void start() {
    if(!display.init()) { fail(display.error(),true); return; }
    if(!game.init()) { fail("No se pudo crear mundo/spawn en PSRAM",true); return; }
    if(!renderer.init()) { fail("Sin PSRAM para tablas de rayos",true); return; }
#ifdef VOXEL_DIAGNOSTIC
    game.diagnostic=true;
#endif
#ifdef VOXEL_BENCHMARK
    if(!benchmark.begin(esp_timer_get_time(),game.world,game.player.camera)) {
        fail("No se pudo iniciar benchmark",true); return;
    }
    game.paused=false;
    ESP_LOGI("voxel","VOXEL_BENCH {\"event\":\"start\",\"version\":\"%s\",\"width\":%d,\"height\":%d,\"seed\":%lu,\"view_distance\":%.1f}",
        voxel::cfg::Version,voxel::cfg::Width,voxel::cfg::Height,
        static_cast<unsigned long>(voxel::cfg::Seed),voxel::cfg::ViewDistance);
    logBenchmarkScene();
#endif
    previous=esp_timer_get_time(); ready=true;
    ESP_LOGI("voxel","Inicio v%s %dx%d mundo=%08lx board=%d",voxel::cfg::Version,voxel::cfg::Width,voxel::cfg::Height,
             static_cast<unsigned long>(game.world.checksum()),static_cast<int>(M5.getBoard()));
}
void frame() {
    if(!ready || failed) { vTaskDelay(pdMS_TO_TICKS(100)); return; }
    const uint64_t now=esp_timer_get_time();
    const float dt=(now-previous)*.000001f; previous=now;
    M5.update();
    voxel::TouchPoint points[5]; std::size_t count=0;
    for(std::size_t i=0;i<M5.Touch.getCount() && count<5;++i) {
        const auto& point=M5.Touch.getDetail(i);
        if(point.isPressed()) points[count++]={point.id,point.x,point.y};
    }
#ifdef VOXEL_BENCHMARK
    switch(benchmark.update(now,game.world,game.player.camera)) {
    case voxel::Benchmark::Event::SceneStarted: logBenchmarkScene(); break;
    case voxel::Benchmark::Event::Measuring:
        ESP_LOGI("voxel","VOXEL_BENCH {\"event\":\"measuring\",\"scene\":\"%s\"}",voxel::Benchmark::name(benchmark.scene())); break;
    case voxel::Benchmark::Event::SceneFinished: logBenchmarkResult(); break;
    case voxel::Benchmark::Event::Finished: ESP_LOGI("voxel","VOXEL_BENCH {\"event\":\"finished\"}"); break;
    default: break;
    }
#else
    const auto controls=input.update(points,count,game.paused || game.diagnostic);
    const bool wasPaused=game.paused,wasDiagnostic=game.diagnostic;
    game.advance(dt,controls);
    if(wasPaused!=game.paused || wasDiagnostic!=game.diagnostic) input.cancelHeld();
#endif
    diagnostics.contacts=static_cast<uint8_t>(count);
    const uint64_t beforeRender=esp_timer_get_time();
    voxel::RenderStats stats;
    voxel::Hud hud(display.image());
    if(game.diagnostic) hud.pattern(frames);
    else stats=renderer.render(game.world,game.player.camera,display.image());
    hud.draw(game,diagnostics);
#ifdef VOXEL_BENCHMARK
    hud.rect(0,9,voxel::cfg::Width,8,0);
    char label[48]; std::snprintf(label,sizeof(label),"BENCH %d/4 %s",std::min(4,benchmark.scene()+1),
        benchmark.finished()?"FIN":(benchmark.measuring()?"MIDIENDO":"PREPARANDO"));
    hud.text(2,10,label);
#endif
    const uint64_t beforePresent=esp_timer_get_time();
    if(!display.present()) {
        // Never scribble error text into an active scanout. Log and leave the
        // last intact frame visible if presentation has already begun.
        fail(display.error(),false); return;
    }
    const uint64_t after=esp_timer_get_time();
    const auto completed=display.takeCompleted();
    diagnostics.record(after,completed,beforePresent-beforeRender,after-beforePresent,stats);
#ifdef VOXEL_BENCHMARK
    benchmark.record(after,completed,beforePresent-beforeRender,after-beforePresent,
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
#endif
    ++frames;
    if(after-lastLog>=5000000) {
        lastLog=after;
        ESP_LOGI("voxel","fps=%.1f render=%.1fms present=%.1fms p95_observado=%.1fms pasos=%.1f psram=%u heap=%u min=%u tactos=%u",
            diagnostics.fps,diagnostics.renderMs,diagnostics.presentMs,diagnostics.frameP95,diagnostics.averageSteps,
            static_cast<unsigned>(diagnostics.freePsram),static_cast<unsigned>(diagnostics.freeInternal),
            static_cast<unsigned>(diagnostics.minInternal),diagnostics.contacts);
    }
    vTaskDelay(1);
}
}
#ifdef ARDUINO
void setup() { start(); }
void loop() { frame(); }
#else
extern "C" void app_main() { start(); for(;;) frame(); }
#endif
