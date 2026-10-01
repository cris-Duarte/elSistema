#include "repository.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include "bsp/m5stack_tab5.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "repository";
constexpr const char* kMainPath = BSP_SPIFFS_MOUNT_POINT "/el_sistema.json";
constexpr const char* kTempPath = BSP_SPIFFS_MOUNT_POINT "/el_sistema.tmp";
constexpr const char* kBackupPath = BSP_SPIFFS_MOUNT_POINT "/el_sistema.bak";

int64_t json_i64(const cJSON* object, const char* name, int64_t fallback = 0)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? static_cast<int64_t>(item->valuedouble) : fallback;
}

uint32_t json_u32(const cJSON* object, const char* name, uint32_t fallback = 0)
{
    const int64_t value = json_i64(object, name, fallback);
    return value < 0 ? fallback : static_cast<uint32_t>(value);
}

bool json_bool(const cJSON* object, const char* name, bool fallback = false)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

std::string json_string(const cJSON* object, const char* name, const char* fallback = "")
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : fallback;
}

void add_i64(cJSON* object, const char* name, int64_t value)
{
    cJSON_AddNumberToObject(object, name, static_cast<double>(value));
}

cJSON* interval_to_json(const ActiveInterval& interval)
{
    cJSON* item = cJSON_CreateObject();
    add_i64(item, "started_at_utc", interval.started_at_utc);
    add_i64(item, "ended_at_utc", interval.ended_at_utc);
    cJSON_AddNumberToObject(item, "duration_seconds", interval.duration_seconds);
    return item;
}

cJSON* mission_to_json(const Mission& mission)
{
    cJSON* item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "id", mission.id);
    cJSON_AddStringToObject(item, "title", mission.title.c_str());
    cJSON_AddStringToObject(item, "description", mission.description.c_str());
    cJSON_AddStringToObject(item, "rank", rank_to_string(mission.rank));
    cJSON_AddStringToObject(item, "status", status_to_string(mission.status));
    cJSON_AddNumberToObject(item, "elapsed_seconds", mission.elapsed_seconds);
    add_i64(item, "created_at_utc", mission.created_at_utc);
    add_i64(item, "updated_at_utc", mission.updated_at_utc);
    add_i64(item, "completed_at_utc", mission.completed_at_utc);
    cJSON_AddNumberToObject(item, "xp_awarded", mission.xp_awarded);

    cJSON* reminder = cJSON_AddObjectToObject(item, "reminder");
    cJSON_AddBoolToObject(reminder, "enabled", mission.reminder.enabled);
    cJSON_AddNumberToObject(reminder, "delay_minutes", mission.reminder.delay_minutes);
    add_i64(reminder, "due_at_utc", mission.reminder.due_at_utc);
    cJSON_AddBoolToObject(reminder, "cancelled_for_pause", mission.reminder.cancelled_for_pause);

    cJSON* intervals = cJSON_AddArrayToObject(item, "intervals");
    for (const auto& interval : mission.intervals) {
        cJSON_AddItemToArray(intervals, interval_to_json(interval));
    }
    return item;
}

bool mission_from_json(const cJSON* item, Mission& mission)
{
    if (!cJSON_IsObject(item)) return false;
    mission.id = json_u32(item, "id");
    mission.title = json_string(item, "title");
    if (mission.id == 0 || mission.title.empty()) return false;
    mission.description = json_string(item, "description");
    mission.rank = rank_from_string(json_string(item, "rank", "E").c_str());
    mission.status = status_from_string(json_string(item, "status", "backlog").c_str());
    mission.elapsed_seconds = json_u32(item, "elapsed_seconds");
    mission.created_at_utc = json_i64(item, "created_at_utc");
    mission.updated_at_utc = json_i64(item, "updated_at_utc");
    mission.completed_at_utc = json_i64(item, "completed_at_utc");
    mission.xp_awarded = json_u32(item, "xp_awarded");

    const cJSON* reminder = cJSON_GetObjectItemCaseSensitive(item, "reminder");
    if (cJSON_IsObject(reminder)) {
        mission.reminder.enabled = json_bool(reminder, "enabled", true);
        mission.reminder.delay_minutes = static_cast<uint16_t>(
            std::clamp<uint32_t>(json_u32(reminder, "delay_minutes", 5), 1, 120));
        mission.reminder.due_at_utc = json_i64(reminder, "due_at_utc");
        mission.reminder.cancelled_for_pause = json_bool(reminder, "cancelled_for_pause");
    }

    const cJSON* intervals = cJSON_GetObjectItemCaseSensitive(item, "intervals");
    const cJSON* interval = nullptr;
    cJSON_ArrayForEach(interval, intervals) {
        ActiveInterval parsed;
        parsed.started_at_utc = json_i64(interval, "started_at_utc");
        parsed.ended_at_utc = json_i64(interval, "ended_at_utc");
        parsed.duration_seconds = json_u32(interval, "duration_seconds");
        mission.intervals.push_back(parsed);
    }
    mission.runtime_started_us = 0;
    mission.interval_started_utc = 0;
    return true;
}

cJSON* state_to_json(const AppState& state)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema_version", state.schema_version);
    cJSON_AddNumberToObject(root, "next_mission_id", state.next_mission_id);

    cJSON* profile = cJSON_AddObjectToObject(root, "profile");
    cJSON_AddNumberToObject(profile, "level", state.profile.level);
    cJSON_AddNumberToObject(profile, "level_xp", state.profile.level_xp);
    add_i64(profile, "total_xp", static_cast<int64_t>(state.profile.total_xp));

    cJSON* settings = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddNumberToObject(settings, "utc_offset_minutes", state.settings.utc_offset_minutes);
    cJSON_AddStringToObject(settings, "timezone", state.settings.timezone.c_str());
    cJSON_AddNumberToObject(settings, "default_reminder_minutes", state.settings.default_reminder_minutes);
    cJSON_AddBoolToObject(settings, "tutorial_completed", state.settings.tutorial_completed);

    cJSON* focus = cJSON_AddObjectToObject(root, "focus_zone");
    cJSON_AddNumberToObject(focus, "active_id", state.focus.active_id);
    cJSON* order = cJSON_AddArrayToObject(focus, "ordered_ids");
    for (MissionId id : state.focus.ordered_ids) cJSON_AddItemToArray(order, cJSON_CreateNumber(id));

    cJSON* missions = cJSON_AddArrayToObject(root, "missions");
    for (const auto& mission : state.missions) cJSON_AddItemToArray(missions, mission_to_json(mission));
    return root;
}

bool state_from_json(const cJSON* root, AppState& state)
{
    if (!cJSON_IsObject(root)) return false;
    const uint32_t version = json_u32(root, "schema_version");
    if (version == 0 || version > AppState::kSchemaVersion) return false;

    AppState parsed;
    parsed.schema_version = AppState::kSchemaVersion;
    parsed.next_mission_id = std::max<uint32_t>(1, json_u32(root, "next_mission_id", 1));

    const cJSON* profile = cJSON_GetObjectItemCaseSensitive(root, "profile");
    parsed.profile.level = std::max<uint32_t>(1, json_u32(profile, "level", 1));
    parsed.profile.level_xp = json_u32(profile, "level_xp");
    parsed.profile.total_xp = static_cast<uint64_t>(json_i64(profile, "total_xp"));

    const cJSON* settings = cJSON_GetObjectItemCaseSensitive(root, "settings");
    parsed.settings.utc_offset_minutes = static_cast<int16_t>(json_i64(settings, "utc_offset_minutes", -180));
    parsed.settings.timezone = json_string(settings, "timezone", "America/Asuncion");
    parsed.settings.default_reminder_minutes = static_cast<uint16_t>(
        std::clamp<uint32_t>(json_u32(settings, "default_reminder_minutes", 5), 1, 120));
    parsed.settings.tutorial_completed = json_bool(settings, "tutorial_completed");

    const cJSON* missions = cJSON_GetObjectItemCaseSensitive(root, "missions");
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, missions) {
        Mission mission;
        if (mission_from_json(item, mission)) {
            parsed.next_mission_id = std::max(parsed.next_mission_id, mission.id + 1);
            parsed.missions.push_back(std::move(mission));
        }
    }

    const cJSON* focus = cJSON_GetObjectItemCaseSensitive(root, "focus_zone");
    parsed.focus.active_id = json_u32(focus, "active_id");
    const cJSON* order = cJSON_GetObjectItemCaseSensitive(focus, "ordered_ids");
    cJSON_ArrayForEach(item, order) {
        if (!cJSON_IsNumber(item)) continue;
        const MissionId id = static_cast<MissionId>(item->valuedouble);
        if (find_mission(parsed, id) != nullptr &&
            std::find(parsed.focus.ordered_ids.begin(), parsed.focus.ordered_ids.end(), id) == parsed.focus.ordered_ids.end()) {
            parsed.focus.ordered_ids.push_back(id);
        }
    }
    if (find_mission(parsed, parsed.focus.active_id) == nullptr) parsed.focus.active_id = 0;
    state = std::move(parsed);
    return true;
}

}  // namespace

void MissionRepository::set_error(const char* message)
{
    last_error_ = message != nullptr ? message : "Error desconocido";
    ESP_LOGE(TAG, "%s", last_error_.c_str());
}

bool MissionRepository::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        set_error("No se pudo iniciar NVS");
        return false;
    }
    if (nvs_open("el_sistema", NVS_READWRITE, &nvs_) != ESP_OK) {
        set_error("No se pudo abrir NVS");
        return false;
    }

    const esp_vfs_spiffs_conf_t spiffs_config = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
        .format_if_mount_failed = true,
    };
    err = esp_vfs_spiffs_register(&spiffs_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        set_error("No se pudo montar ni recuperar SPIFFS");
        return false;
    }
    mounted_ = true;
    return true;
}

bool MissionRepository::load_file(const char* path, AppState& state)
{
    FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return false;
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::rewind(file);
    if (length <= 0 || length > 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    std::string data(static_cast<size_t>(length), '\0');
    const size_t read = std::fread(data.data(), 1, data.size(), file);
    std::fclose(file);
    if (read != data.size()) return false;
    cJSON* root = cJSON_ParseWithLength(data.c_str(), data.size());
    if (root == nullptr) return false;
    const bool ok = state_from_json(root, state);
    cJSON_Delete(root);
    return ok;
}

bool MissionRepository::load(AppState& state)
{
    if (!mounted_) return false;
    if (!load_file(kMainPath, state)) {
        ESP_LOGW(TAG, "El archivo principal no existe o no es válido; buscando respaldo");
        if (!load_file(kBackupPath, state)) return false;
        ESP_LOGW(TAG, "Estado recuperado desde el respaldo");
        save(state);
    }
    load_preferences(state);
    if (restore_checkpoint(state)) save(state);
    return true;
}

bool MissionRepository::write_file(const char* path, const AppState& state)
{
    cJSON* root = state_to_json(state);
    if (root == nullptr) return false;
    char* text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == nullptr) return false;

    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        cJSON_free(text);
        return false;
    }
    const size_t length = std::strlen(text);
    const bool ok = std::fwrite(text, 1, length, file) == length && std::fflush(file) == 0 && fsync(fileno(file)) == 0;
    std::fclose(file);
    cJSON_free(text);
    return ok;
}

bool MissionRepository::save(const AppState& state)
{
    if (!mounted_) return false;
    if (!write_file(kTempPath, state)) {
        std::remove(kTempPath);
        set_error("No se pudo escribir el estado; los datos anteriores siguen intactos");
        return false;
    }
    std::remove(kBackupPath);
    struct stat info {};
    if (::stat(kMainPath, &info) == 0 && std::rename(kMainPath, kBackupPath) != 0) {
        std::remove(kTempPath);
        set_error("No se pudo preparar el respaldo");
        return false;
    }
    if (std::rename(kTempPath, kMainPath) != 0) {
        std::rename(kBackupPath, kMainPath);
        set_error("No se pudo activar el nuevo estado");
        return false;
    }
    save_preferences(state);
    last_error_.clear();
    return true;
}

void MissionRepository::load_preferences(AppState& state)
{
    if (nvs_ == 0) return;
    int16_t offset = state.settings.utc_offset_minutes;
    uint16_t reminder = state.settings.default_reminder_minutes;
    uint8_t tutorial = state.settings.tutorial_completed ? 1 : 0;
    char timezone[48] = {};
    size_t timezone_length = sizeof(timezone);
    nvs_get_i16(nvs_, "utc_offset", &offset);
    nvs_get_u16(nvs_, "reminder", &reminder);
    nvs_get_u8(nvs_, "tutorial", &tutorial);
    if (nvs_get_str(nvs_, "timezone", timezone, &timezone_length) == ESP_OK && timezone[0] != '\0') {
        state.settings.timezone = timezone;
    }
    state.settings.utc_offset_minutes = offset;
    state.settings.default_reminder_minutes = std::clamp<uint16_t>(reminder, 1, 120);
    state.settings.tutorial_completed = tutorial != 0;
}

void MissionRepository::save_preferences(const AppState& state)
{
    if (nvs_ == 0) return;
    nvs_set_i16(nvs_, "utc_offset", state.settings.utc_offset_minutes);
    nvs_set_str(nvs_, "timezone", state.settings.timezone.c_str());
    nvs_set_u16(nvs_, "reminder", state.settings.default_reminder_minutes);
    nvs_set_u8(nvs_, "tutorial", state.settings.tutorial_completed ? 1 : 0);
    nvs_commit(nvs_);
}

bool MissionRepository::save_checkpoint(MissionId active_id, uint32_t elapsed_seconds, int64_t interval_started_utc)
{
    if (nvs_ == 0) return false;
    if (nvs_set_u32(nvs_, "cp_id", active_id) != ESP_OK ||
        nvs_set_u32(nvs_, "cp_elapsed", elapsed_seconds) != ESP_OK ||
        nvs_set_i64(nvs_, "cp_started", interval_started_utc) != ESP_OK || nvs_commit(nvs_) != ESP_OK) {
        set_error("No se pudo guardar el checkpoint del cronómetro");
        return false;
    }
    return true;
}

bool MissionRepository::restore_checkpoint(AppState& state)
{
    if (nvs_ == 0) return false;
    uint32_t id = 0;
    uint32_t elapsed = 0;
    int64_t started_at = 0;
    const bool has_checkpoint = nvs_get_u32(nvs_, "cp_id", &id) == ESP_OK && id != 0;
    if (has_checkpoint) {
        nvs_get_u32(nvs_, "cp_elapsed", &elapsed);
        nvs_get_i64(nvs_, "cp_started", &started_at);
    } else {
        id = state.focus.active_id;
    }
    Mission* mission = find_mission(state, id);
    if (mission != nullptr && mission->status != MissionStatus::Completed) {
        const uint32_t recovered_interval = elapsed > mission->elapsed_seconds ? elapsed - mission->elapsed_seconds : 0;
        if (has_checkpoint && recovered_interval > 0) {
            mission->intervals.push_back({started_at, started_at > 0 ? started_at + recovered_interval : 0,
                                          recovered_interval});
        }
        mission->elapsed_seconds = std::max(mission->elapsed_seconds, elapsed);
        mission->status = MissionStatus::FocusPaused;
        mission->runtime_started_us = 0;
        mission->interval_started_utc = 0;
        mission->reminder.due_at_utc = 0;
        mission->reminder.cancelled_for_pause = false;
        if (std::find(state.focus.ordered_ids.begin(), state.focus.ordered_ids.end(), id) == state.focus.ordered_ids.end()) {
            state.focus.ordered_ids.insert(state.focus.ordered_ids.begin(), id);
        }
    }
    for (auto& candidate : state.missions) {
        if (candidate.status == MissionStatus::FocusActive) candidate.status = MissionStatus::FocusPaused;
    }
    state.focus.active_id = 0;
    nvs_erase_key(nvs_, "cp_id");
    nvs_erase_key(nvs_, "cp_elapsed");
    nvs_erase_key(nvs_, "cp_started");
    nvs_commit(nvs_);
    return mission != nullptr;
}

uint8_t MissionRepository::storage_percent() const
{
    if (!mounted_) return 100;
    size_t total = 0;
    size_t used = 0;
    if (esp_spiffs_info(CONFIG_BSP_SPIFFS_PARTITION_LABEL, &total, &used) != ESP_OK || total == 0) return 100;
    return static_cast<uint8_t>(std::min<size_t>(100, used * 100 / total));
}

}  // namespace el_sistema
