#include "DisplayBackend.h"
#include "Memory.h"
#include "Scale.h"
#include "M5Unified.h"
#include "lgfx/v1/platforms/esp32p4/Panel_DSI.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace voxel {
static_assert(std::atomic<uint32_t>::is_always_lock_free,"ISR counter must be lock-free");
DisplayBackend::~DisplayBackend() {
    // The global hardware backend has process lifetime. Never tear down a panel
    // with an ISR still referring to this instance.
    if(panel_) { esp_lcd_dpi_panel_event_callbacks_t cb{}; esp_lcd_dpi_panel_register_event_callbacks(panel_,&cb,nullptr); }
    for(auto* p:images_) freeExternal(p);
}
bool IRAM_ATTR DisplayBackend::onRefresh(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void* context) {
    static_cast<DisplayBackend*>(context)->refresh_.fetch_add(1,std::memory_order_relaxed);
    return false;
}
bool DisplayBackend::init() {
    auto config=M5.config();
    config.internal_mic=false; config.internal_spk=false;
    config.internal_imu=false; config.internal_rtc=false;
    config.external_imu=false; config.external_rtc=false;
    config.output_power=false;
    config.fallback_board=m5::board_t::board_M5Tab5;
    M5.begin(config);
    if(M5.getBoard()!=m5::board_t::board_M5Tab5) { error_="Tab5 no detectado"; return false; }
    M5.Display.setRotation(1);
    M5.Display.setBrightness(160);
    if(M5.Display.width()!=cfg::ScreenW || M5.Display.height()!=cfg::ScreenH) {
        error_="Geometria de display inesperada"; return false;
    }
    // A tiny, version-checked patch adds this getter to M5GFX. It does not
    // replace initialization, reset tables, touch detection or the DSI driver.
    auto* dsi=static_cast<lgfx::Panel_DSI*>(M5.Display.getPanel());
    panel_=dsi->voxelDpiPanel();
    const auto c=dsi->config(); panelW_=c.panel_width; panelH_=c.panel_height;
    const unsigned r=M5.Display.getRotation();
    rotation_=((r+c.offset_rotation)&3)|((r&4)^(c.offset_rotation&4));
    void* fb0=nullptr; void* fb1=nullptr;
    if(!panel_ || esp_lcd_dpi_panel_get_frame_buffer(panel_,2,&fb0,&fb1)!=ESP_OK || !fb0 || !fb1 || fb0==fb1) {
        error_="Doble framebuffer DSI no disponible"; return false;
    }
    native_[0]=static_cast<uint16_t*>(fb0); native_[1]=static_cast<uint16_t*>(fb1);
    for(auto& p:images_) {
        p=static_cast<uint16_t*>(allocateExternal(cfg::Pixels*sizeof(uint16_t)));
        if(!p) { error_="Sin PSRAM para imagenes"; return false; }
    }
    esp_lcd_dpi_panel_event_callbacks_t cb{}; cb.on_refresh_done=onRefresh;
    if(esp_lcd_dpi_panel_register_event_callbacks(panel_,&cb,this)!=ESP_OK) {
        error_="No se pudo registrar refresh DSI"; return false;
    }
    return true;
}
bool DisplayBackend::waitPending() {
    if(!pending_) return true;
    const int64_t start=esp_timer_get_time();
    // IDF 5.4.2 restarts DMA with cur_fb_index BEFORE on_refresh_done.
    // Two events after submission conservatively cover a callback racing that
    // submission, and ensure the old scanout is no longer being read.
    while(static_cast<uint32_t>(refresh_.load(std::memory_order_relaxed)-pendingAfter_)<2) {
        if(esp_timer_get_time()-start>250000) { error_="Timeout de refresh DSI"; return false; }
        vTaskDelay(1);
    }
    front_=pendingIndex_; pending_=false; ++completed_;
    return true;
}
bool DisplayBackend::present() {
    if(!panel_ || !waitPending()) return false;
    const int back=1-front_;
    if(!scaleNearest(images_[imageIndex_],cfg::Width,cfg::Height,native_[back],panelW_,panelH_,rotation_)) {
        error_="Escalado/orientacion invalido"; return false;
    }
    // Passing the actual native FB pointer uses IDF's no-copy path, including
    // cache C2M writeback. Do not call M5.Display drawing APIs after this point:
    // its line pointers still target fb0, which can now be the active scanout.
    if(esp_lcd_panel_draw_bitmap(panel_,0,0,panelW_,panelH_,native_[back])!=ESP_OK) {
        error_="Error al presentar framebuffer"; return false;
    }
    pendingAfter_=refresh_.load(std::memory_order_relaxed);
    pendingIndex_=back; pending_=true; imageIndex_=1-imageIndex_;
    return true;
}
uint32_t DisplayBackend::takeCompleted() {
    if(pending_ && static_cast<uint32_t>(refresh_.load(std::memory_order_relaxed)-pendingAfter_)>=2) {
        front_=pendingIndex_; pending_=false; ++completed_;
    }
    const auto count=completed_; completed_=0; return count;
}
}
