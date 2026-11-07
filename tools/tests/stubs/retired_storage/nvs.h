#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
using nvs_handle_t = uint32_t;
constexpr int NVS_READONLY=0, NVS_READWRITE=1;
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_erase_all(nvs_handle_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
