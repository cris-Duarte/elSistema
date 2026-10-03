#include "connectivity.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>

#include "cJSON.h"
#include "esp_hosted_api.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "connectivity";
constexpr const char* kNamespace = "dictation_net";
constexpr const char* kConfigKey = "config_v2";
constexpr const char* kCorruptConfigKey = "config_bad";
constexpr uint32_t kConfigSchema = 2;
constexpr EventBits_t kConnected = BIT0;
constexpr EventBits_t kFailed = BIT1;
constexpr std::array<uint32_t, 5> kRetrySeconds {2, 5, 15, 30, 60};

std::string read_string(nvs_handle_t handle, const char* key)
{
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0) return {};
    std::string value(length, '\0');
    if (nvs_get_str(handle, key, value.data(), &length) != ESP_OK) return {};
    if (!value.empty() && value.back() == '\0') value.pop_back();
    return value;
}

std::string json_string(const cJSON* object, const char* key)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : "";
}

bool json_bool(const cJSON* object, const char* key, bool fallback = false)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

std::string txt_value(const mdns_result_t* result, const char* key)
{
    if (result == nullptr || result->txt == nullptr) return {};
    for (size_t index = 0; index < result->txt_count; ++index) {
        if (result->txt[index].key != nullptr && std::strcmp(result->txt[index].key, key) == 0 &&
            result->txt[index].value != nullptr) return result->txt[index].value;
    }
    return {};
}

std::string first_ipv4(const mdns_result_t* result)
{
    char buffer[IP4ADDR_STRLEN_MAX] = {};
    for (const mdns_ip_addr_t* address = result == nullptr ? nullptr : result->addr; address != nullptr;
         address = address->next) {
        if (address->addr.type != ESP_IPADDR_TYPE_V4) continue;
        esp_ip4addr_ntoa(&address->addr.u_addr.ip4, buffer, sizeof(buffer));
        return buffer;
    }
    return {};
}

}  // namespace

std::string encode_network_config(const NetworkConfigData& config)
{
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) return {};
    cJSON_AddNumberToObject(root, "schema", kConfigSchema);
    cJSON* array = cJSON_AddArrayToObject(root, "profiles");
    for (const NetworkProfile& profile : config.profiles) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", profile.ssid.c_str());
        cJSON_AddStringToObject(item, "password", profile.password.c_str());
        cJSON_AddNumberToObject(item, "priority", profile.priority);
        cJSON_AddBoolToObject(item, "hidden", profile.hidden);
        cJSON_AddItemToArray(array, item);
    }
    cJSON* server = cJSON_AddObjectToObject(root, "server");
    cJSON_AddStringToObject(server, "id", config.preferred_server_id.c_str());
    cJSON_AddStringToObject(server, "name", config.preferred_server_name.c_str());
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == nullptr) return {};
    std::string result = printed;
    cJSON_free(printed);
    return result;
}

bool decode_network_config(const char* json, NetworkConfigData& config)
{
    cJSON* root = cJSON_Parse(json);
    const cJSON* schema = root == nullptr ? nullptr : cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON* profiles = root == nullptr ? nullptr : cJSON_GetObjectItemCaseSensitive(root, "profiles");
    if (!cJSON_IsNumber(schema) || schema->valueint != static_cast<int>(kConfigSchema) ||
        !cJSON_IsArray(profiles)) {
        if (root != nullptr) cJSON_Delete(root);
        return false;
    }
    NetworkConfigData parsed;
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, profiles) {
        if (!cJSON_IsObject(item) || parsed.profiles.size() >= ConnectivityService::kMaxProfiles) continue;
        NetworkProfile profile;
        profile.ssid = json_string(item, "ssid");
        profile.password = json_string(item, "password");
        const cJSON* priority = cJSON_GetObjectItemCaseSensitive(item, "priority");
        profile.priority = cJSON_IsNumber(priority)
            ? static_cast<uint8_t>(std::clamp(priority->valueint, 0, 7))
            : static_cast<uint8_t>(parsed.profiles.size());
        profile.hidden = json_bool(item, "hidden");
        if (!profile.ssid.empty() && profile.ssid.size() <= 32 && profile.password.size() <= 63)
            parsed.profiles.push_back(std::move(profile));
    }
    const cJSON* server = cJSON_GetObjectItemCaseSensitive(root, "server");
    parsed.preferred_server_id = cJSON_IsObject(server) ? json_string(server, "id") : "";
    parsed.preferred_server_name = cJSON_IsObject(server) ? json_string(server, "name") : "";
    cJSON_Delete(root);
    config = std::move(parsed);
    return true;
}

NetworkConfigData migrate_legacy_network_config(const std::string& ssid, const std::string& password)
{
    NetworkConfigData config;
    if (!ssid.empty()) config.profiles.push_back({ssid, password, 0, false});
    return config;
}

uint32_t connectivity_retry_delay_seconds(size_t attempt)
{
    return kRetrySeconds[std::min(attempt, kRetrySeconds.size() - 1)];
}

std::vector<DiscoveredServer> deduplicate_discovered_servers(const std::vector<DiscoveredServer>& servers)
{
    std::vector<DiscoveredServer> unique;
    unique.reserve(servers.size());
    for (const DiscoveredServer& server : servers) {
        if (server.id.empty()) continue;
        const auto duplicate = std::find_if(unique.begin(), unique.end(), [&](const DiscoveredServer& existing) {
            return existing.id == server.id;
        });
        if (duplicate == unique.end()) unique.push_back(server);
    }
    return unique;
}

std::vector<size_t> rank_network_candidates(const std::vector<NetworkProfile>& profiles,
                                            const std::vector<WifiNetwork>& networks)
{
    struct Candidate { size_t index; int rssi; bool visible; };
    std::vector<Candidate> candidates;
    for (size_t index = 0; index < profiles.size(); ++index) {
        const auto match = std::find_if(networks.begin(), networks.end(), [&](const WifiNetwork& network) {
            return network.ssid == profiles[index].ssid;
        });
        if (match != networks.end()) candidates.push_back({index, match->rssi, true});
        else if (profiles[index].hidden) candidates.push_back({index, -128, false});
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& left, const Candidate& right) {
        const NetworkProfile& a = profiles[left.index];
        const NetworkProfile& b = profiles[right.index];
        if (a.priority != b.priority) return a.priority < b.priority;
        if (left.visible != right.visible) return left.visible;
        return left.rssi > right.rssi;
    });
    std::vector<size_t> result;
    result.reserve(candidates.size());
    for (const Candidate& candidate : candidates) result.push_back(candidate.index);
    return result;
}

std::string choose_server_id(const std::string& preferred_id,
                             const std::vector<DiscoveredServer>& servers,
                             bool& selection_required)
{
    const auto preferred = std::find_if(servers.begin(), servers.end(), [&](const DiscoveredServer& server) {
        return server.id == preferred_id;
    });
    if (preferred != servers.end()) {
        selection_required = false;
        return preferred->id;
    }
    if (preferred_id.empty() && servers.size() == 1) {
        selection_required = false;
        return servers.front().id;
    }
    selection_required = !servers.empty() && (servers.size() > 1 || !preferred_id.empty());
    return {};
}

const char* connectivity_phase_name(ConnectivityPhase phase)
{
    switch (phase) {
        case ConnectivityPhase::Unconfigured: return "SIN WIFI";
        case ConnectivityPhase::Idle: return "SIN CONEXIÓN";
        case ConnectivityPhase::Starting: return "INICIANDO WIFI";
        case ConnectivityPhase::Scanning: return "BUSCANDO RED";
        case ConnectivityPhase::Connecting: return "CONECTANDO";
        case ConnectivityPhase::Connected: return "WIFI CONECTADO";
        case ConnectivityPhase::DiscoveringServer: return "BUSCANDO MAC";
        case ConnectivityPhase::Ready: return "LISTO";
        case ConnectivityPhase::NoKnownNetwork: return "SIN RED CONOCIDA";
        case ConnectivityPhase::ConnectionFailed: return "SIN CONEXIÓN";
        case ConnectivityPhase::ServerMissing: return "MAC NO DISPONIBLE";
        case ConnectivityPhase::Error: return "ERROR DE RED";
    }
    return "SIN CONEXIÓN";
}

bool ConnectivityService::load()
{
    if (mutex_ == nullptr) mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        last_error_ = "No se pudo crear el estado de conectividad";
        return false;
    }
    nvs_handle_t handle = 0;
    const esp_err_t opened = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (opened != ESP_OK) {
        last_error_ = "No se pudo abrir el almacén de redes";
        return false;
    }
    const std::string config = read_string(handle, kConfigKey);
    bool recovered = false;
    bool ok = config.empty() ? migrate_legacy(handle) : parse_config(config.c_str());
    if (!ok && !config.empty()) {
        recovered = recover_corrupt_config(handle, config);
        ok = recovered;
    }
    nvs_close(handle);
    loaded_ = ok;
    update_public_profiles();
    set_state(profiles_.empty() ? ConnectivityPhase::Unconfigured : ConnectivityPhase::Idle,
              recovered ? "Se restableció una configuración de red dañada"
                        : profiles_.empty() ? "Añade una red para usar el dictado" : "Redes conocidas cargadas",
              recovered ? "Las redes dañadas se conservaron en NVS para diagnóstico" : "");
    return ok;
}

bool ConnectivityService::recover_corrupt_config(uint32_t nvs_handle, const std::string& corrupt_config)
{
    const nvs_handle_t handle = static_cast<nvs_handle_t>(nvs_handle);
    profiles_.clear();
    preferred_server_id_.clear();
    preferred_server_name_.clear();
    const std::string empty_config = serialize_config();
    esp_err_t err = nvs_set_str(handle, kCorruptConfigKey, corrupt_config.c_str());
    if (err == ESP_OK) err = nvs_set_str(handle, kConfigKey, empty_config.c_str());
    if (err == ESP_OK) err = nvs_commit(handle);
    if (err != ESP_OK) {
        last_error_ = "La configuración de redes está dañada y no se pudo restablecer";
        return false;
    }
    ESP_LOGW(TAG, "Configuración de conectividad dañada; se restablecieron los perfiles");
    return true;
}

bool ConnectivityService::migrate_legacy(uint32_t nvs_handle)
{
    const nvs_handle_t handle = static_cast<nvs_handle_t>(nvs_handle);
    const std::string ssid = read_string(handle, "ssid");
    NetworkConfigData migrated = migrate_legacy_network_config(ssid, read_string(handle, "password"));
    profiles_ = std::move(migrated.profiles);
    preferred_server_id_ = std::move(migrated.preferred_server_id);
    preferred_server_name_ = std::move(migrated.preferred_server_name);
    const std::string json = serialize_config();
    esp_err_t err = json.empty() ? ESP_ERR_NO_MEM : nvs_set_str(handle, kConfigKey, json.c_str());
    const char* legacy_keys[] = {"ssid", "password", "server_ip", "port"};
    for (const char* key : legacy_keys) {
        if (err != ESP_OK) break;
        err = nvs_erase_key(handle, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    if (err != ESP_OK) {
        last_error_ = "No se pudo migrar la red anterior";
        return false;
    }
    if (!ssid.empty()) ESP_LOGI(TAG, "Configuración de red anterior migrada a v2");
    return true;
}

bool ConnectivityService::parse_config(const char* json)
{
    NetworkConfigData config;
    if (!decode_network_config(json, config)) {
        last_error_ = "La configuración de redes está dañada o no es compatible";
        return false;
    }
    profiles_ = std::move(config.profiles);
    preferred_server_id_ = std::move(config.preferred_server_id);
    preferred_server_name_ = std::move(config.preferred_server_name);
    std::stable_sort(profiles_.begin(), profiles_.end(), [](const NetworkProfile& left, const NetworkProfile& right) {
        return left.priority < right.priority;
    });
    last_error_.clear();
    return true;
}

std::string ConnectivityService::serialize_config() const
{
    return encode_network_config({profiles_, preferred_server_id_, preferred_server_name_});
}

bool ConnectivityService::persist()
{
    const std::string json = serialize_config();
    if (json.empty()) {
        last_error_ = "No se pudo preparar la configuración de redes";
        return false;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_str(handle, kConfigKey, json.c_str());
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK) {
        last_error_ = "No se pudo guardar la configuración de redes";
        return false;
    }
    return true;
}

bool ConnectivityService::init()
{
    if (initialized_) return true;
    if (!loaded_ && !load()) return false;
    if (events_ == nullptr) events_ = xEventGroupCreate();
    if (events_ == nullptr) {
        last_error_ = "No se pudo crear el estado de Wi‑Fi";
        return false;
    }
    set_state(ConnectivityPhase::Starting, "Iniciando enlace con el ESP32‑C6");
    esp_err_t err = ESP_OK;
    if (!network_stack_ready_) {
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            set_state(ConnectivityPhase::Error, {}, "Falló esp_netif_init");
            return false;
        }
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            set_state(ConnectivityPhase::Error, {}, "Falló el bucle de eventos Wi‑Fi");
            return false;
        }
        station_netif_ = esp_netif_create_default_wifi_sta();
        if (station_netif_ == nullptr) {
            set_state(ConnectivityPhase::Error, {}, "No se pudo crear la interfaz Wi‑Fi remota");
            return false;
        }
        network_stack_ready_ = true;
    }
    if (!wifi_driver_ready_.load()) {
        err = esp_hosted_init();
        if (err != ESP_OK) {
            set_state(ConnectivityPhase::Error, {}, std::string("No se pudo iniciar P4–C6: ") + esp_err_to_name(err));
            return false;
        }
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&config);
        if (err != ESP_OK) {
            set_state(ConnectivityPhase::Error, {}, std::string("ESP‑Hosted/C6 no respondió: ") + esp_err_to_name(err));
            return false;
        }
        wifi_driver_ready_.store(true);
    }
    if (wifi_handler_ == nullptr &&
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, this, &wifi_handler_) != ESP_OK) {
        set_state(ConnectivityPhase::Error, {}, "No se pudieron registrar los eventos Wi‑Fi");
        return false;
    }
    if (ip_handler_ == nullptr &&
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, this, &ip_handler_) != ESP_OK) {
        set_state(ConnectivityPhase::Error, {}, "No se pudieron registrar los eventos IP");
        return false;
    }
    if ((err = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK ||
        (err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK || (err = esp_wifi_start()) != ESP_OK) {
        set_state(ConnectivityPhase::Error, {}, std::string("No se pudo iniciar Wi‑Fi: ") + esp_err_to_name(err));
        return false;
    }
    err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        set_state(ConnectivityPhase::Error, {}, std::string("No se pudo iniciar Bonjour: ") + esp_err_to_name(err));
        return false;
    }
    mdns_hostname_set("el-sistema-tab5");
    mdns_ready_ = true;
    initialized_ = true;
    set_state(profiles_.empty() ? ConnectivityPhase::NoKnownNetwork : ConnectivityPhase::Idle,
              profiles_.empty() ? "Añade una red para usar el dictado" : "Wi‑Fi preparado");
    ESP_LOGI(TAG, "ESP32-C6 y mDNS disponibles");
    return true;
}

void ConnectivityService::start_background()
{
    background_started_ = true;
    // El enlace P4-C6 también transporta Bluetooth HCI. Se inicia aunque aún
    // no haya perfiles Wi-Fi para que BLE pueda servir en el alta inicial.
    if (!initialized_ && !init()) {
        schedule_retry();
        return;
    }
    schedule_retry(true);
}

void ConnectivityService::maintain()
{
    if (!background_started_) return;
    const int64_t now = esp_timer_get_time();
    if (!connected()) {
        const ConnectivitySnapshot current = snapshot();
        if (!current.current_ssid.empty()) {
            resolved_server_url_.clear();
            next_discovery_us_ = 0;
            if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                snapshot_.current_ssid.clear();
                snapshot_.current_rssi = -127;
                snapshot_.servers.clear();
                for (KnownNetwork& known : snapshot_.profiles) known.current = false;
                ++snapshot_.generation;
                xSemaphoreGive(mutex_);
            }
            set_state(ConnectivityPhase::ConnectionFailed, "Conexión perdida; buscando otra red");
            schedule_retry(true);
        }
        if (!profiles_.empty() && now >= next_retry_us_) {
            if (auto_connect()) schedule_retry(true);
            else schedule_retry();
        }
        return;
    }
    if (now >= next_discovery_us_) discover_servers();
}

void ConnectivityService::schedule_retry(bool reset)
{
    if (reset) retry_step_ = 0;
    next_retry_us_ = reset ? 0 : esp_timer_get_time() +
        static_cast<int64_t>(connectivity_retry_delay_seconds(retry_step_)) * 1000000LL;
    if (!reset && retry_step_ + 1 < kRetrySeconds.size()) ++retry_step_;
}

void ConnectivityService::event_handler(void* arg, const char* base, int32_t id, void*)
{
    auto* service = static_cast<ConnectivityService*>(arg);
    if (service == nullptr || service->events_ == nullptr) return;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupClearBits(service->events_, kFailed);
        xEventGroupSetBits(service->events_, kConnected);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(service->events_, kConnected);
        if (service->ignore_next_disconnect_.exchange(false)) return;
        xEventGroupSetBits(service->events_, kFailed);
    }
}

ConnectivitySnapshot ConnectivityService::snapshot() const
{
    ConnectivitySnapshot result;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(30)) == pdTRUE) {
        result = snapshot_;
        xSemaphoreGive(mutex_);
    }
    return result;
}

bool ConnectivityService::configured() const { return !snapshot().profiles.empty(); }
bool ConnectivityService::connected() const
{
    return events_ != nullptr && (xEventGroupGetBits(events_) & kConnected) != 0;
}
bool ConnectivityService::ready() const { return snapshot().ready(); }

void ConnectivityService::set_state(ConnectivityPhase phase, const std::string& message, const std::string& error)
{
    last_error_ = error;
    if (mutex_ == nullptr) return;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.phase = phase;
        snapshot_.message = message.empty() ? connectivity_phase_name(phase) : message;
        snapshot_.error = error;
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
}

void ConnectivityService::update_public_profiles()
{
    if (mutex_ == nullptr) return;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.profiles.clear();
        for (const NetworkProfile& profile : profiles_) {
            snapshot_.profiles.push_back({profile.ssid, profile.priority, profile.hidden,
                                          profile.ssid == snapshot_.current_ssid});
        }
        snapshot_.selected_server_id = preferred_server_id_;
        snapshot_.selected_server_name = preferred_server_name_;
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
}

bool ConnectivityService::validate_credentials(const NetworkCredentials& credentials)
{
    if (credentials.ssid.empty() || credentials.ssid.size() > 32) {
        last_error_ = "El SSID debe contener entre 1 y 32 bytes";
        return false;
    }
    if (credentials.password.size() > 63 || (!credentials.password.empty() && credentials.password.size() < 8)) {
        last_error_ = "La contraseña debe contener entre 8 y 63 caracteres";
        return false;
    }
    return true;
}

bool ConnectivityService::scan()
{
    if (!initialized_ && !init()) return false;
    set_state(ConnectivityPhase::Scanning, "Buscando redes disponibles");
    wifi_scan_config_t config {};
    config.show_hidden = true;
    esp_err_t err = esp_wifi_scan_start(&config, true);
    if (err != ESP_OK) {
        set_state(ConnectivityPhase::Error, {}, std::string("No se pudo escanear: ") + esp_err_to_name(err));
        return false;
    }
    uint16_t count = 0;
    err = esp_wifi_scan_get_ap_num(&count);
    count = std::min<uint16_t>(count, 24);
    std::vector<wifi_ap_record_t> records(count);
    if (err == ESP_OK && count > 0) err = esp_wifi_scan_get_ap_records(&count, records.data());
    if (err != ESP_OK) {
        set_state(ConnectivityPhase::Error, {}, "No se pudieron leer las redes encontradas");
        return false;
    }
    std::map<std::string, WifiNetwork> unique;
    for (uint16_t index = 0; index < count; ++index) {
        const char* raw = reinterpret_cast<const char*>(records[index].ssid);
        if (raw[0] == '\0') continue;
        const std::string ssid(raw, strnlen(raw, sizeof(records[index].ssid)));
        const bool known = std::any_of(profiles_.begin(), profiles_.end(), [&](const NetworkProfile& profile) {
            return profile.ssid == ssid;
        });
        WifiNetwork found {ssid, records[index].rssi, records[index].authmode != WIFI_AUTH_OPEN, known};
        auto previous = unique.find(ssid);
        if (previous == unique.end() || previous->second.rssi < found.rssi) unique[ssid] = std::move(found);
    }
    std::vector<WifiNetwork> networks;
    for (auto& pair : unique) networks.push_back(std::move(pair.second));
    std::sort(networks.begin(), networks.end(), [](const WifiNetwork& left, const WifiNetwork& right) {
        return left.rssi > right.rssi;
    });
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.networks = std::move(networks);
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
    const ConnectivitySnapshot current = snapshot();
    set_state(connected() ? (resolved_server_url_.empty() ? ConnectivityPhase::Connected : ConnectivityPhase::Ready)
                          : ConnectivityPhase::Idle,
              current.networks.empty() ? "No se encontraron redes" : "Redes actualizadas");
    return true;
}

bool ConnectivityService::connect_profile(const NetworkProfile& profile, uint32_t timeout_ms)
{
    set_state(ConnectivityPhase::Connecting, "Conectando a " + profile.ssid);
    wifi_config_t config {};
    std::memcpy(config.sta.ssid, profile.ssid.data(), profile.ssid.size());
    std::strncpy(reinterpret_cast<char*>(config.sta.password), profile.password.c_str(),
                 sizeof(config.sta.password) - 1);
    config.sta.threshold.authmode = profile.password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    ignore_next_disconnect_.store(connected());
    esp_wifi_disconnect();
    xEventGroupClearBits(events_, kConnected | kFailed);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (err == ESP_OK) err = esp_wifi_connect();
    if (err != ESP_OK) {
        last_error_ = std::string("No se pudo iniciar la conexión: ") + esp_err_to_name(err);
        return false;
    }
    const EventBits_t result = xEventGroupWaitBits(events_, kConnected | kFailed, pdFALSE, pdFALSE,
                                                    pdMS_TO_TICKS(timeout_ms));
    if ((result & kConnected) == 0) {
        last_error_ = (result & kFailed) ? "La red rechazó la conexión" : "Tiempo de conexión agotado";
        return false;
    }
    wifi_ap_record_t access_point {};
    const int8_t rssi = esp_wifi_sta_get_ap_info(&access_point) == ESP_OK ? access_point.rssi : -127;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.current_ssid = profile.ssid;
        snapshot_.current_rssi = rssi;
        snapshot_.servers.clear();
        for (KnownNetwork& known : snapshot_.profiles) known.current = known.ssid == profile.ssid;
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
    retry_step_ = 0;
    discovery_retry_step_ = 0;
    next_discovery_us_ = 0;
    set_state(ConnectivityPhase::Connected, "Conectado a " + profile.ssid);
    return true;
}

bool ConnectivityService::auto_connect(bool user_requested)
{
    if (!initialized_ && !init()) return false;
    if (profiles_.empty()) {
        set_state(ConnectivityPhase::NoKnownNetwork, "No hay redes conocidas");
        return false;
    }
    scan();
    const ConnectivitySnapshot visible = snapshot();
    const std::vector<size_t> candidates = rank_network_candidates(profiles_, visible.networks);
    for (const size_t candidate : candidates) {
        if (connect_profile(profiles_[candidate], 10000)) {
            discover_servers();
            return true;
        }
    }
    const std::string detail = candidates.empty() ? "Ninguna red conocida está al alcance" : last_error_;
    set_state(ConnectivityPhase::ConnectionFailed,
              user_requested ? "No fue posible conectar" : "Se reintentará automáticamente", detail);
    return false;
}

bool ConnectivityService::connect_and_save(const NetworkCredentials& credentials)
{
    if (!validate_credentials(credentials)) {
        set_state(ConnectivityPhase::Error, {}, last_error_);
        return false;
    }
    auto existing = std::find_if(profiles_.begin(), profiles_.end(), [&](const NetworkProfile& profile) {
        return profile.ssid == credentials.ssid;
    });
    if (existing == profiles_.end() && profiles_.size() >= kMaxProfiles) {
        set_state(ConnectivityPhase::Error, {}, "Puedes guardar como máximo 8 redes");
        return false;
    }
    if (!initialized_ && !init()) return false;
    const uint8_t priority = existing == profiles_.end() ? static_cast<uint8_t>(profiles_.size()) : existing->priority;
    NetworkProfile candidate {credentials.ssid, credentials.password, priority, credentials.hidden};
    if (!connect_profile(candidate, 15000)) {
        set_state(ConnectivityPhase::ConnectionFailed, "La red no se guardó", last_error_);
        schedule_retry();
        return false;
    }
    const std::vector<NetworkProfile> previous_profiles = profiles_;
    if (existing == profiles_.end()) profiles_.push_back(candidate);
    else *existing = candidate;
    normalize_priorities();
    if (!persist()) {
        profiles_ = previous_profiles;
        update_public_profiles();
        set_state(ConnectivityPhase::Error, {}, last_error_);
        return false;
    }
    update_public_profiles();
    set_state(ConnectivityPhase::Connected, "Red conectada y guardada");
    discover_servers();
    return true;
}

bool ConnectivityService::connect_known(size_t index)
{
    if (index >= profiles_.size()) return false;
    if (!initialized_ && !init()) return false;
    if (!connect_profile(profiles_[index], 15000)) {
        set_state(ConnectivityPhase::ConnectionFailed, "No fue posible conectar", last_error_);
        schedule_retry();
        return false;
    }
    discover_servers();
    return true;
}

void ConnectivityService::normalize_priorities()
{
    std::stable_sort(profiles_.begin(), profiles_.end(), [](const NetworkProfile& left, const NetworkProfile& right) {
        return left.priority < right.priority;
    });
    for (size_t index = 0; index < profiles_.size(); ++index) profiles_[index].priority = static_cast<uint8_t>(index);
}

ConnectivityPhase ConnectivityService::stable_connected_phase() const
{
    if (!connected()) return profiles_.empty() ? ConnectivityPhase::NoKnownNetwork : ConnectivityPhase::Idle;
    if (!resolved_server_url_.empty()) return ConnectivityPhase::Ready;
    const ConnectivityPhase current = snapshot().phase;
    if (current == ConnectivityPhase::DiscoveringServer || current == ConnectivityPhase::ServerMissing)
        return current;
    return ConnectivityPhase::Connected;
}

bool ConnectivityService::forget_network(size_t index)
{
    if (index >= profiles_.size()) return false;
    const bool active = snapshot().current_ssid == profiles_[index].ssid;
    const std::vector<NetworkProfile> previous_profiles = profiles_;
    profiles_.erase(profiles_.begin() + static_cast<std::ptrdiff_t>(index));
    normalize_priorities();
    if (!persist()) {
        profiles_ = previous_profiles;
        update_public_profiles();
        set_state(ConnectivityPhase::Error, {}, last_error_);
        return false;
    }
    if (active && initialized_) {
        ignore_next_disconnect_.store(false);
        esp_wifi_disconnect();
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            snapshot_.current_ssid.clear();
            snapshot_.current_rssi = -127;
            snapshot_.servers.clear();
            ++snapshot_.generation;
            xSemaphoreGive(mutex_);
        }
        schedule_retry(true);
    }
    update_public_profiles();
    set_state(active ? (profiles_.empty() ? ConnectivityPhase::NoKnownNetwork : ConnectivityPhase::Idle)
                     : stable_connected_phase(),
              profiles_.empty() ? "No hay redes conocidas" : "Red olvidada");
    return true;
}

bool ConnectivityService::move_network(size_t index, int delta)
{
    if (index >= profiles_.size()) return false;
    const int target = std::clamp<int>(static_cast<int>(index) + delta, 0, static_cast<int>(profiles_.size()) - 1);
    if (target == static_cast<int>(index)) return true;
    const std::vector<NetworkProfile> previous_profiles = profiles_;
    std::swap(profiles_[index], profiles_[static_cast<size_t>(target)]);
    for (size_t position = 0; position < profiles_.size(); ++position)
        profiles_[position].priority = static_cast<uint8_t>(position);
    if (!persist()) {
        profiles_ = previous_profiles;
        set_state(ConnectivityPhase::Error, {}, last_error_);
        return false;
    }
    update_public_profiles();
    set_state(stable_connected_phase(), "Prioridad actualizada");
    return true;
}

bool ConnectivityService::discover_servers(bool user_requested)
{
    if (!connected() || !mdns_ready_) return false;
    set_state(ConnectivityPhase::DiscoveringServer, "Buscando El Sistema en la red");
    mdns_result_t* results = nullptr;
    const esp_err_t err = mdns_query_ptr("_elsistema", "_tcp", 3000, 8, &results);
    std::vector<DiscoveredServer> servers;
    if (err == ESP_OK) {
        for (mdns_result_t* result = results; result != nullptr; result = result->next) {
            DiscoveredServer server;
            server.id = txt_value(result, "id");
            server.path = txt_value(result, "path");
            const std::string version = txt_value(result, "version");
            server.protocol_version = version.empty() ? 0 :
                static_cast<uint16_t>(std::strtoul(version.c_str(), nullptr, 10));
            server.name = result->instance_name == nullptr ? "MacBook" : result->instance_name;
            server.ipv4 = first_ipv4(result);
            server.port = result->port;
            if (server.id.empty() || server.ipv4.empty() || server.port == 0 || server.path != "/inference" ||
                server.protocol_version != 1) continue;
            servers.push_back(std::move(server));
        }
    }
    if (results != nullptr) mdns_query_results_free(results);
    servers = deduplicate_discovered_servers(servers);
    ESP_LOGI(TAG, "Bonjour encontró %u servidor(es) compatible(s)",
             static_cast<unsigned>(servers.size()));
    bool selection_required = false;
    const std::string selected_id = choose_server_id(preferred_server_id_, servers, selection_required);
    if (preferred_server_id_.empty() && !selected_id.empty()) {
        const auto selected = std::find_if(servers.begin(), servers.end(), [&](const DiscoveredServer& server) {
            return server.id == selected_id;
        });
        preferred_server_id_ = selected->id;
        preferred_server_name_ = selected->name;
        if (!persist()) ESP_LOGW(TAG, "No se pudo recordar el servidor seleccionado automáticamente");
    }
    for (DiscoveredServer& server : servers) server.selected = server.id == preferred_server_id_;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.servers = std::move(servers);
        snapshot_.selected_server_id = preferred_server_id_;
        snapshot_.selected_server_name = preferred_server_name_;
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
    if (resolve_selected_server()) {
        discovery_retry_step_ = 0;
        next_discovery_us_ = esp_timer_get_time() + 60000000LL;
        set_state(ConnectivityPhase::Ready, "Dictado conectado a " + preferred_server_name_);
        return true;
    }
    const ConnectivitySnapshot current = snapshot();
    next_discovery_us_ = esp_timer_get_time() +
        static_cast<int64_t>(connectivity_retry_delay_seconds(discovery_retry_step_)) * 1000000LL;
    if (discovery_retry_step_ + 1 < kRetrySeconds.size()) ++discovery_retry_step_;
    const std::string message = current.servers.empty() ? "Inicia servidor.sh en el MacBook"
                                : selection_required ? "Elige el MacBook que usarás"
                                                     : "El MacBook preferido no está disponible";
    set_state(ConnectivityPhase::ServerMissing, message,
              user_requested && err != ESP_OK ? std::string("Falló Bonjour: ") + esp_err_to_name(err) : "");
    return false;
}

bool ConnectivityService::resolve_selected_server()
{
    const ConnectivitySnapshot current = snapshot();
    const auto selected = std::find_if(current.servers.begin(), current.servers.end(),
                                       [&](const DiscoveredServer& server) {
        return server.id == preferred_server_id_;
    });
    if (selected == current.servers.end()) {
        resolved_server_url_.clear();
        return false;
    }
    resolved_server_url_ = "http://" + selected->ipv4 + ":" + std::to_string(selected->port) + selected->path;
    return true;
}

bool ConnectivityService::select_server(const std::string& id)
{
    const ConnectivitySnapshot current = snapshot();
    const auto selected = std::find_if(current.servers.begin(), current.servers.end(),
                                       [&](const DiscoveredServer& server) {
        return server.id == id;
    });
    if (selected == current.servers.end()) {
        set_state(ConnectivityPhase::ServerMissing, "Ese MacBook ya no está disponible");
        return false;
    }
    const std::string previous_id = preferred_server_id_;
    const std::string previous_name = preferred_server_name_;
    preferred_server_id_ = selected->id;
    preferred_server_name_ = selected->name;
    if (!persist()) {
        preferred_server_id_ = previous_id;
        preferred_server_name_ = previous_name;
        set_state(ConnectivityPhase::Error, {}, last_error_);
        return false;
    }
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.selected_server_id = preferred_server_id_;
        snapshot_.selected_server_name = preferred_server_name_;
        for (DiscoveredServer& server : snapshot_.servers) server.selected = server.id == preferred_server_id_;
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
    resolve_selected_server();
    set_state(ConnectivityPhase::Ready, "Dictado conectado a " + preferred_server_name_);
    return true;
}

bool ConnectivityService::ensure_ready(uint32_t)
{
    if (!connected() && !auto_connect(true)) return false;
    return discover_servers(true);
}

std::string ConnectivityService::inference_url() const { return resolved_server_url_; }

void ConnectivityService::invalidate_server(const std::string& error)
{
    resolved_server_url_.clear();
    next_discovery_us_ = 0;
    set_state(ConnectivityPhase::ServerMissing, "Se volverá a buscar el MacBook", error);
}

bool ConnectivityService::test_server(uint32_t timeout_ms)
{
    if (!ensure_ready()) return false;
    std::string url = inference_url();
    const size_t path = url.rfind("/inference");
    if (path != std::string::npos) url.erase(path);
    url += "/";
    esp_http_client_config_t config {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = static_cast<int>(timeout_ms);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        invalidate_server("No se pudo crear la prueba HTTP");
        return false;
    }
    const esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK || status < 200 || status >= 400) {
        invalidate_server("El servidor Whisper no respondió");
        return false;
    }
    set_state(ConnectivityPhase::Ready, "Whisper respondió correctamente");
    return true;
}

}  // namespace el_sistema
