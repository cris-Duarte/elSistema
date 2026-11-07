#include "retired_storage.hpp"
#include "nvs.h"
#include "modules.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
using Store = std::map<std::string,std::map<std::string,std::string>>;
static Store saved;
static std::map<std::string,std::string> pending;
static int failure=0, erased=0, writes=0, closed=0;
esp_err_t nvs_open(const char* space,int mode,nvs_handle_t* handle) {
    assert(std::string(space)=="lyrics");
    if(failure==1 || (failure==2&&mode==NVS_READWRITE))return ESP_FAIL;
    if(!saved.count(space))return ESP_ERR_NVS_NOT_FOUND;
    *handle=mode+1;pending=saved.at(space);return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* out,size_t* size) {
    assert(h==1&&out==nullptr);
    auto it=saved.at("lyrics").find(key);
    if(it==saved.at("lyrics").end())return ESP_ERR_NVS_NOT_FOUND;
    *size=it->second.size();return ESP_OK;
}
esp_err_t nvs_erase_all(nvs_handle_t h) {
    assert(h==2);++erased;
    if(failure==3)return ESP_ERR_NO_MEM;
    pending.clear();return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h==2);++writes;
    if(failure==4)return ESP_ERR_NO_MEM;
    saved["lyrics"]=pending;return ESP_OK;
}
void nvs_close(nvs_handle_t){++closed;pending.clear();}
int main() {
    using namespace el_sistema;
    assert(kModules.size()==5);
    for(int id:{2,3,5,7,8,9})assert(!find_module(static_cast<ModuleId>(id)));
    const Store unrelated={{"habits",{{"volume","30"}}},{"dictation_net",{{"profiles","preserved"}}},
                           {"tetris",{{"game","preserved"}}},{"mission",{{"checkpoint","preserved"}}}};
    saved=unrelated;
    assert(clear_retired_module_storage()==ESP_OK&&saved==unrelated&&writes==0&&erased==0);
    saved["lyrics"]={{"account","synthetic-test-credential"},{"prefs","synthetic-preferences"}};
    const auto original=saved;
    for(int mode=1;mode<=4;++mode) {
        saved=original;failure=mode;
        assert(clear_retired_module_storage()!=ESP_OK&&saved==original);
    }
    failure=0;saved=original;
    assert(clear_retired_module_storage()==ESP_OK&&saved.at("lyrics").empty());
    saved.erase("lyrics");assert(saved==unrelated);
    saved["lyrics"]={};const auto previous_writes=writes;
    assert(clear_retired_module_storage()==ESP_OK&&writes==previous_writes);
    assert(closed>0);
    std::puts("Datos retirados: aislamiento NVS, fallos, reintento e idempotencia OK; cinco módulos activos");
}
