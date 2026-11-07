#include "services.hpp"
#include <algorithm>
#include <cstring>
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_hosted_api.h"
#include "esp_log.h"
#include "freertos/task.h"
namespace piensaware {
bool Preferences::load() {
 nvs_handle_t h; if(nvs_open("piensaware",NVS_READWRITE,&h)!=ESP_OK) return false;
 int32_t b=72; uint8_t m=0; nvs_get_i32(h,"brightness",&b); nvs_get_u8(h,"reduced",&m);
 brightness=std::clamp(int(b),10,100); reduced_motion=m!=0; writable=true; nvs_close(h); return true;
}
bool Preferences::save() const {
 if(!writable) return false;
 nvs_handle_t h; if(nvs_open("piensaware",NVS_READWRITE,&h)!=ESP_OK) return false;
 esp_err_t e=nvs_set_i32(h,"brightness",brightness); if(e==ESP_OK)e=nvs_set_u8(h,"reduced",reduced_motion);
 if(e==ESP_OK){e=nvs_commit(h);}
 nvs_close(h); return e==ESP_OK;
}
void WifiService::status(const std::string& s,bool busy) { xSemaphoreTake(mutex_,portMAX_DELAY); state_.status=s; state_.busy=busy; ++state_.revision; xSemaphoreGive(mutex_); }
WifiSnapshot WifiService::snapshot() { if(!mutex_)return {}; xSemaphoreTake(mutex_,portMAX_DELAY); auto s=state_; xSemaphoreGive(mutex_); return s; }
void WifiService::publish_profiles() { xSemaphoreTake(mutex_,portMAX_DELAY); state_.saved.clear(); for(auto&p:profiles_)state_.saved.push_back(p.ssid); ++state_.revision; xSemaphoreGive(mutex_); }
void WifiService::load() {
 if(!storage_ready_){return;}
 nvs_handle_t h; if(nvs_open("piensaware_net",NVS_READONLY,&h)!=ESP_OK)return;
 size_t len=0; nvs_get_str(h,"profiles",nullptr,&len);
 if(len>0 && len<8192) { std::string s(len,'\0'); if(nvs_get_str(h,"profiles",s.data(),&len)==ESP_OK) {
 cJSON* root=cJSON_Parse(s.c_str()); if(cJSON_IsArray(root)) { cJSON* item; cJSON_ArrayForEach(item,root) {
 auto a=cJSON_GetObjectItem(item,"ssid"), b=cJSON_GetObjectItem(item,"password");
 if(cJSON_IsString(a)&&cJSON_IsString(b)&&strlen(a->valuestring)>0&&strlen(a->valuestring)<=32&&strlen(b->valuestring)<=64&&profiles_.size()<8)profiles_.push_back({a->valuestring,b->valuestring});
 }} cJSON_Delete(root); }} nvs_close(h);
}
bool WifiService::persist() {
 if(!storage_ready_)return false;
 cJSON* root=cJSON_CreateArray(); if(!root)return false;
 for(auto&p:profiles_){auto item=cJSON_CreateObject(); cJSON_AddStringToObject(item,"ssid",p.ssid.c_str());cJSON_AddStringToObject(item,"password",p.password.c_str());cJSON_AddItemToArray(root,item);}
 char* json=cJSON_PrintUnformatted(root); cJSON_Delete(root); if(!json)return false;
 nvs_handle_t h; esp_err_t e=nvs_open("piensaware_net",NVS_READWRITE,&h);
 if(e==ESP_OK){ e=nvs_set_str(h,"profiles",json); if(e==ESP_OK)e=nvs_commit(h); nvs_close(h); } cJSON_free(json); return e==ESP_OK;
}
bool WifiService::start(bool storage_ready) {
 storage_ready_=storage_ready; mutex_=xSemaphoreCreateMutex(); commands_=xQueueCreate(4,sizeof(Command));
 if(!mutex_||!commands_)return false;
 return xTaskCreate(worker,"piensa_wifi",8192,this,3,nullptr)==pdPASS;
}
bool WifiService::submit(int kind,const std::string& ssid,const std::string& password) {
 if(!commands_||ssid.size()>32||password.size()>64)return false;
 Command c{};c.kind=kind;memcpy(c.ssid,ssid.c_str(),ssid.size());memcpy(c.password,password.c_str(),password.size());
 return xQueueSend(commands_,&c,0)==pdTRUE;
}
bool WifiService::scan(){return submit(0);}
bool WifiService::connect(const std::string&s,const std::string&p){return !s.empty()&&submit(1,s,p);}
bool WifiService::forget(const std::string&s){return submit(2,s);}
void WifiService::event(void* arg,esp_event_base_t base,int32_t id,void*) {
 auto self=static_cast<WifiService*>(arg);
 if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP) self->linked_=true;
 if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED)self->linked_=false;
}
bool WifiService::initialize() {
 status("Preparando Wi-Fi…",true);
 esp_err_t e=esp_netif_init(); if(e!=ESP_OK&&e!=ESP_ERR_INVALID_STATE)return false;
 e=esp_event_loop_create_default(); if(e!=ESP_OK&&e!=ESP_ERR_INVALID_STATE)return false;
 if(!esp_netif_create_default_wifi_sta())return false;
 if(esp_hosted_init()!=ESP_OK)return false;
 wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT(); if(esp_wifi_init(&cfg)!=ESP_OK)return false;
 if(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,this)!=ESP_OK || esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,this)!=ESP_OK)return false;
 return esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK&&esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK&&esp_wifi_start()==ESP_OK;
}
void WifiService::search() {
 status("Buscando redes…",true); wifi_scan_config_t cfg{};
 if(esp_wifi_scan_start(&cfg,true)!=ESP_OK){status("No se pudieron buscar redes. Intenta otra vez.");return;}
 uint16_t count=30; std::vector<wifi_ap_record_t> aps(count);
 if(esp_wifi_scan_get_ap_records(&count,aps.data())!=ESP_OK){status("No se pudo leer la búsqueda.");return;}
 std::vector<Network> networks;
 for(unsigned i=0;i<count;i++){ std::string ssid(reinterpret_cast<char*>(aps[i].ssid),strnlen(reinterpret_cast<char*>(aps[i].ssid),32)); if(ssid.empty())continue;
 if(std::none_of(networks.begin(),networks.end(),[&](auto&n){return n.ssid==ssid;}))networks.push_back({ssid,aps[i].rssi,aps[i].authmode!=WIFI_AUTH_OPEN}); }
 xSemaphoreTake(mutex_,portMAX_DELAY);state_.networks=std::move(networks);++state_.revision;xSemaphoreGive(mutex_);
 status(linked_?"Conectado":"Elige una red");
}
bool WifiService::associate(const Profile& p) {
 connecting_=true; linked_=false; esp_wifi_disconnect();
 vTaskDelay(pdMS_TO_TICKS(150));
 wifi_config_t cfg{};memcpy(cfg.sta.ssid,p.ssid.data(),p.ssid.size());memcpy(cfg.sta.password,p.password.data(),p.password.size());
 status("Conectando a "+p.ssid+"…",true);
 bool ok=esp_wifi_set_config(WIFI_IF_STA,&cfg)==ESP_OK&&esp_wifi_connect()==ESP_OK;
 if(ok){for(int i=0;i<150&&!linked_;i++)vTaskDelay(pdMS_TO_TICKS(100));ok=linked_;}
 connecting_=false;
 xSemaphoreTake(mutex_,portMAX_DELAY);state_.ssid=ok?p.ssid:"";++state_.revision;xSemaphoreGive(mutex_);
 if(!ok){esp_wifi_disconnect();}
 status(ok?"Conectado":"No se pudo conectar. Revisa la contraseña y la señal.");return ok;
}
void WifiService::worker(void* p){static_cast<WifiService*>(p)->run();}
void WifiService::run() {
 load();publish_profiles();initialized_=initialize();
 if(!initialized_)status("Wi-Fi no disponible. Puedes seguir sin conexión.");
 else status("Sin conexión");
 TickType_t retry=0; size_t next=0;
 while(true){
 Command c{};
 if(xQueueReceive(commands_,&c,pdMS_TO_TICKS(250))==pdTRUE){
 if(c.kind==2){auto old=profiles_;profiles_.erase(std::remove_if(profiles_.begin(),profiles_.end(),[&](auto&p){return p.ssid==c.ssid;}),profiles_.end());
 if(!persist()){profiles_=old;status("No se pudo olvidar la red.");}else{
 if(snapshot().ssid==c.ssid){esp_wifi_disconnect();linked_=false;xSemaphoreTake(mutex_,portMAX_DELAY);state_.ssid.clear();xSemaphoreGive(mutex_);}
 publish_profiles();status("Red olvidada");} }
 else if(!initialized_)status("Wi-Fi no disponible. Reinicia para intentar otra vez.");
 else if(c.kind==0)search();
 else {
 Profile p{c.ssid,c.password};auto found=std::find_if(profiles_.begin(),profiles_.end(),[&](auto&v){return v.ssid==p.ssid;});
 if(c.kind==3&&found!=profiles_.end())p=*found;
 if(associate(p)){auto old=profiles_;if(found!=profiles_.end())*found=p;else if(profiles_.size()<8)profiles_.push_back(p);else status("Conectado. Olvida una red para guardar esta.");
 if(!persist()){profiles_=old;status("Conectado, pero no se pudo guardar la red.");}publish_profiles();}
 }
 memset(c.password,0,sizeof(c.password));retry=xTaskGetTickCount()+pdMS_TO_TICKS(30000);
 }
 if(initialized_&&!linked_&&!profiles_.empty()&&int32_t(xTaskGetTickCount()-retry)>=0){
 associate(profiles_[next++%profiles_.size()]);retry=xTaskGetTickCount()+pdMS_TO_TICKS(30000);
 } else if(initialized_&&!linked_&&!snapshot().ssid.empty()) {xSemaphoreTake(mutex_,portMAX_DELAY);state_.ssid.clear();xSemaphoreGive(mutex_);status("Conexión perdida. Reconectando…");}
 }
}
}
