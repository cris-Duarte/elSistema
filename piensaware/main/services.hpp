#pragma once
#include <string>
#include <vector>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "nvs.h"
namespace piensaware {
struct Preferences {
 int brightness=72; bool reduced_motion=false; bool writable=false;
 bool load(); bool save() const;
};
struct Network { std::string ssid; int rssi=0; bool secured=true; };
struct WifiSnapshot { std::string status="Sin conexión", ssid; std::vector<Network> networks; std::vector<std::string> saved; bool busy=false; unsigned revision=0; };
class WifiService {
public:
 bool start(bool storage_ready);
 bool scan(); bool connect_known(const std::string& s){return submit(3,s);} bool connect(const std::string&,const std::string&); bool forget(const std::string&);
 WifiSnapshot snapshot();
private:
 struct Profile { std::string ssid,password; };
 struct Command { int kind; char ssid[33]; char password[65]; };
 QueueHandle_t commands_=nullptr; SemaphoreHandle_t mutex_=nullptr;
 WifiSnapshot state_; std::vector<Profile> profiles_; bool storage_ready_=false, initialized_=false;
 std::atomic<bool> linked_{false}; std::atomic<bool> connecting_{false};
 static void worker(void*); void run(); bool initialize(); bool associate(const Profile&); void search();
 bool persist(); void load(); void status(const std::string&,bool busy=false); void publish_profiles();
 static void event(void*,esp_event_base_t,int32_t,void*);
 bool submit(int,const std::string& ssid = {},const std::string& password = {});
};
}
