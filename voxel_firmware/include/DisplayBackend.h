#pragma once
#include "Config.h"
#include <atomic>
#include <cstdint>
#ifdef ESP_PLATFORM
#include "esp_lcd_mipi_dsi.h"
#endif
namespace voxel {
class DisplayBackend {
public:
    ~DisplayBackend();
    bool init();
    uint16_t* image() { return images_[imageIndex_]; }
    bool present();
    uint32_t takeCompleted();
    const char* error() const { return error_; }
private:
    uint16_t* images_[2]{};
    uint16_t* native_[2]{};
    int imageIndex_=0,front_=0,pendingIndex_=1;
    int panelW_=0,panelH_=0;
    unsigned rotation_=0;
    bool pending_=false;
    uint32_t pendingAfter_=0;
    std::atomic<uint32_t> refresh_{0};
    uint32_t completed_=0;
    const char* error_="";
#ifdef ESP_PLATFORM
    esp_lcd_panel_handle_t panel_=nullptr;
    static bool onRefresh(esp_lcd_panel_handle_t,esp_lcd_dpi_panel_event_data_t*,void*);
#endif
    bool waitPending();
};
}
