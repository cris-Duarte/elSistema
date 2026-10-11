#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
struct bsp_codec_config_t {
    esp_err_t (*i2s_read)(void*, size_t, size_t*, uint32_t);
    esp_err_t (*i2s_write)(void*, size_t, size_t*, uint32_t);
    esp_err_t (*set_in_gain)(float);
};
bool bsp_codec_try_init();
bool bsp_codec_ready();
bsp_codec_config_t* bsp_get_codec_handle();
