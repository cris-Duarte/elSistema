#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include "radar_model.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

namespace el_sistema {

struct NetworkCredentials {
    std::string ssid;
    std::string password;
    bool hidden = false;
};

struct NetworkProfile {
    std::string ssid;
    std::string password;
    uint8_t priority = 0;
    bool hidden = false;
};

struct KnownNetwork {
    std::string ssid;
    uint8_t priority = 0;
    bool hidden = false;
    bool current = false;
};

struct WifiNetwork {
    std::string ssid;
    int8_t rssi = -127;
    bool secured = true;
    bool known = false;
};

struct DiscoveredServer {
    std::string id;
    std::string name;
    std::string ipv4;
    uint16_t port = 0;
    std::string path = "/inference";
    uint16_t protocol_version = 1;
    bool selected = false;
};

struct NetworkConfigData {
    std::vector<NetworkProfile> profiles;
    std::string preferred_server_id;
    std::string preferred_server_name;
};

std::string encode_network_config(const NetworkConfigData& config);
bool decode_network_config(const char* json, NetworkConfigData& config);
NetworkConfigData migrate_legacy_network_config(const std::string& ssid, const std::string& password);
uint32_t connectivity_retry_delay_seconds(size_t attempt);
std::vector<DiscoveredServer> deduplicate_discovered_servers(const std::vector<DiscoveredServer>& servers);
std::vector<size_t> rank_network_candidates(const std::vector<NetworkProfile>& profiles,
                                            const std::vector<WifiNetwork>& networks);
std::string choose_server_id(const std::string& preferred_id,
                             const std::vector<DiscoveredServer>& servers,
                             bool& selection_required);

enum class ConnectivityPhase : uint8_t {
    Unconfigured,
    Idle,
    Starting,
    Scanning,
    Connecting,
    Connected,
    DiscoveringServer,
    Ready,
    NoKnownNetwork,
    ConnectionFailed,
    ServerMissing,
    Error,
};

struct ConnectivitySnapshot {
    ConnectivityPhase phase = ConnectivityPhase::Unconfigured;
    std::vector<KnownNetwork> profiles;
    std::vector<WifiNetwork> networks;
    std::vector<DiscoveredServer> servers;
    std::string current_ssid;
    int8_t current_rssi = -127;
    std::string selected_server_id;
    std::string selected_server_name;
    std::string message;
    std::string error;
    uint32_t generation = 0;

    bool configured() const { return !profiles.empty(); }
    bool connected() const { return !current_ssid.empty(); }
    bool ready() const { return phase == ConnectivityPhase::Ready; }
    bool server_selection_required() const
    {
        if (!connected() || servers.empty()) return false;
        const bool preferred_present = !selected_server_id.empty() &&
            std::any_of(servers.begin(), servers.end(), [this](const DiscoveredServer& server) {
                return server.id == selected_server_id;
            });
        return !preferred_present && (servers.size() > 1 || !selected_server_id.empty());
    }
};

const char* connectivity_phase_name(ConnectivityPhase phase);

class ConnectivityService {
public:
    static constexpr size_t kMaxProfiles = 8;

    bool load();
    bool init();
    void start_background();
    void maintain();

    ConnectivitySnapshot snapshot() const;
    bool configured() const;
    bool connected() const;
    bool ready() const;
    bool hosted_ready() const { return wifi_driver_ready_.load(); }

    bool scan();
    // Called only by the existing network worker. Does not change connection
    // state or persist detections, and retains BSSID/channel/hidden records.
    bool scan_observations(std::vector<RadarObservation>& observations, std::string& error);
    bool auto_connect(bool user_requested = false);
    bool connect_and_save(const NetworkCredentials& credentials);
    bool connect_known(size_t index);
    bool forget_network(size_t index);
    bool move_network(size_t index, int delta);

    bool discover_servers(bool user_requested = false);
    bool select_server(const std::string& id);
    bool ensure_ready(uint32_t timeout_ms = 15000);
    bool test_server(uint32_t timeout_ms = 5000);
    std::string inference_url() const;
    const std::string& last_error() const { return last_error_; }

private:
    static void event_handler(void* arg, const char* base, int32_t id, void* data);

    bool persist();
    bool recover_corrupt_config(uint32_t nvs_handle, const std::string& corrupt_config);
    bool migrate_legacy(uint32_t nvs_handle);
    bool parse_config(const char* json);
    std::string serialize_config() const;
    bool validate_credentials(const NetworkCredentials& credentials);
    bool connect_profile(const NetworkProfile& profile, uint32_t timeout_ms);
    void normalize_priorities();
    ConnectivityPhase stable_connected_phase() const;
    void schedule_retry(bool reset = false);
    void set_state(ConnectivityPhase phase, const std::string& message = {}, const std::string& error = {});
    void update_public_profiles();
    bool resolve_selected_server();
    void invalidate_server(const std::string& error);

    mutable SemaphoreHandle_t mutex_ = nullptr;
    std::vector<NetworkProfile> profiles_;
    ConnectivitySnapshot snapshot_;
    std::string preferred_server_id_;
    std::string preferred_server_name_;
    std::string resolved_server_url_;

    EventGroupHandle_t events_ = nullptr;
    esp_netif_t* station_netif_ = nullptr;
    esp_event_handler_instance_t wifi_handler_ = nullptr;
    esp_event_handler_instance_t ip_handler_ = nullptr;
    bool network_stack_ready_ = false;
    std::atomic<bool> wifi_driver_ready_ {false};
    bool mdns_ready_ = false;
    bool initialized_ = false;
    bool loaded_ = false;
    bool background_started_ = false;
    std::atomic<bool> ignore_next_disconnect_ {false};
    int64_t next_retry_us_ = 0;
    int64_t next_discovery_us_ = 0;
    uint8_t retry_step_ = 0;
    uint8_t discovery_retry_step_ = 0;
    std::string last_error_;
};

}  // namespace el_sistema
