#pragma once
#ifdef PIENSAWARE_HOST
#include "host_services.hpp"
#else
#include "hardware.hpp"
#include "services.hpp"
#endif
#include "apps.hpp"
namespace piensaware {
class Shell {
 friend struct ShellProbe;
public:
 Shell(Preferences& p,WifiService& w,ClockService& c,OrientationService& o,PowerService& power,AppRegistry& apps):prefs_(p),wifi_(w),clock_(c),orientation_(o),power_(power),apps_(apps){}
 void start(lv_display_t*);
 void stop();
 ~Shell(){stop();}
private:
 Preferences& prefs_; WifiService& wifi_; ClockService& clock_; OrientationService& orientation_; PowerService& power_; AppRegistry& apps_;
 lv_display_t* display_=nullptr; lv_obj_t* root_=nullptr; lv_obj_t* clock_label_=nullptr;lv_obj_t* status_=nullptr;lv_obj_t* wifi_list_=nullptr;lv_obj_t* message_=nullptr;lv_obj_t* overlay_=nullptr;
 lv_obj_t* password_=nullptr; lv_obj_t* keyboard_=nullptr;
 lv_timer_t* boot_timer_=nullptr; lv_timer_t* tick_timer_=nullptr;
 int page_=0, rotation_=1; bool boot_=true,locked_=false;unsigned wifi_revision_=~0u;
 std::string chosen_ssid_; const AppDescriptor* active_=nullptr;
 void clear(int page); void header(const char*); void home();void settings();void network();void refresh_network();void password(const Network&);void date_time();void tick();void open_app(const AppDescriptor&);
 void notice(const char*);void dismiss();
};
}
