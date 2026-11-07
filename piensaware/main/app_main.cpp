#include "ui.hpp"
#include "m5_platform.hpp"
#include "bsp/m5stack_tab5.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
namespace {
piensaware::Preferences prefs;
piensaware::WifiService wifi;
piensaware::ClockService clock_service;
piensaware::OrientationService orientation;
piensaware::PowerService power;
piensaware::AppRegistry apps;
piensaware::Shell shell(prefs,wifi,clock_service,orientation,power,apps);
}
extern "C" void app_main(){
 ESP_LOGI("piensaware","Piensaware 0.1.0 iniciando");
 // Never erase shared NVS, even when initialization reports a full/incompatible partition.
 bool storage=nvs_flash_init()==ESP_OK;
 if(storage)prefs.load();else ESP_LOGW("piensaware","NVS no disponible; preferencias temporales");
 bool m5=piensaware::init_m5_platform();if(!m5)ESP_ERROR_CHECK(bsp_i2c_init());
 bsp_io_expander_pi4ioe_init(bsp_i2c_get_handle());bsp_set_charge_en(true);bsp_reset_tp();
 clock_service.init(bsp_i2c_get_handle());if(m5)orientation.init(bsp_i2c_get_handle());power.init(bsp_i2c_get_handle());
 bsp_display_cfg_t cfg={.lvgl_port_cfg=ESP_LVGL_PORT_INIT_CONFIG(),.buffer_size=BSP_LCD_H_RES*BSP_LCD_V_RES,.double_buffer=true,.flags={.buff_dma=false,.buff_spiram=true,.sw_rotate=true}};
 auto display=bsp_display_start_with_config(&cfg);if(!display){ESP_LOGE("piensaware","No se pudo iniciar la pantalla");return;}
 bsp_display_lock(0);lv_display_set_rotation(display,LV_DISPLAY_ROTATION_90);shell.start(display);bsp_display_unlock();
 bsp_display_brightness_set(prefs.brightness);bsp_display_backlight_on();
 if(!wifi.start(storage))ESP_LOGW("piensaware","No se pudo iniciar la tarea Wi-Fi");
 while(true){ESP_LOGI("piensaware","Salud: heap=%u psram=%u",unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));vTaskDelay(pdMS_TO_TICKS(30000));}
}
