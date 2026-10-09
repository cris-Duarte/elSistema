#include "hourglass_repository.hpp"
#include "esp_log.h"

namespace el_sistema {
bool HourglassRepository::init()
{
    return nvs_open("hourglass", NVS_READWRITE, &handle_) == ESP_OK;
}
bool HourglassRepository::load(HourglassSnapshot& state, bool& corrupt)
{
    corrupt = false;
    if (!handle_) return false;
    HourglassRecord record{};
    size_t size = record.size();
    const esp_err_t err = nvs_get_blob(handle_, "session", record.data(), &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) return false;
    if (err != ESP_OK || size != record.size() || !decode_hourglass(record, state)) {
        corrupt = true;
        ESP_LOGW("hourglass", "Checkpoint inválido; se conservan los valores iniciales");
        return false;
    }
    return true;
}
bool HourglassRepository::save(const HourglassSnapshot& state)
{
    if (!handle_) return false;
    const HourglassRecord record = encode_hourglass(state);
    return nvs_set_blob(handle_, "session", record.data(), record.size()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}
} // namespace el_sistema
