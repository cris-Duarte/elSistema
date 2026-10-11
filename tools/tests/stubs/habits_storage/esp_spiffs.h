#pragma once
#include <cstddef>
#include <cstring>
constexpr int ESP_OK = 0;
// IDF matches a mounted-by-label filesystem only when the label is supplied.
inline int esp_spiffs_info(const char* label, size_t* total, size_t* used) {
    if (!label || std::strcmp(label, "storage") != 0) return -1;
    *total = 2 * 1024 * 1024;
    *used = 8192;
    return ESP_OK;
}
