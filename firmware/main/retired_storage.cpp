#include "retired_storage.hpp"
#include "nvs.h"

namespace el_sistema {
esp_err_t clear_retired_module_storage()
{
    // ID 9 was retired. This namespace held its account and preferences only.
    // Opening read-only first avoids creating it on installations without it.
    constexpr const char* space = "lyrics";
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(space, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    size_t size = 0;
    const auto account = nvs_get_blob(handle, "account", nullptr, &size);
    const auto preferences = nvs_get_blob(handle, "prefs", nullptr, &size);
    nvs_close(handle);
    if (account == ESP_ERR_NVS_NOT_FOUND && preferences == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;

    result = nvs_open(space, NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}
}
