#include "radar.hpp"
#include <algorithm>
#include <cmath>
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

namespace el_sistema {
RadarService::RadarService(DictationService& network, BleService& ble, AudioService& audio, AudioSessionManager& sessions)
    : network_(network), ble_(ble), audio_(audio), sessions_(sessions) {}
bool RadarService::init()
{
    mutex_ = xSemaphoreCreateMutex(); commands_ = xQueueCreate(16, sizeof(Message));
    if (!mutex_ || !commands_) return false;
    nvs_handle_t nvs;
    if (nvs_open("radar", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t version = 0, filter = 0, sound = 0;
        if (nvs_get_u8(nvs, "version", &version) == ESP_OK && version == 1) {
            nvs_get_u8(nvs, "filter", &filter); nvs_get_u8(nvs, "sound", &sound);
            if (filter <= 2) model_.filter(static_cast<RadarFilter>(filter));
            model_.sound(sound == 1);
        }
        nvs_close(nvs);
    }
    snapshot_ = model_.snapshot();
    return xTaskCreate(task_entry, "radar", 8192, this, 3, &task_) == pdPASS;
}
bool RadarService::send(Command command, uint32_t value)
{ const Message message {command, value}; return commands_ && xQueueSend(commands_, &message, 0) == pdTRUE; }
bool RadarService::visible(bool on) { if (!on) sessions_.cancel_radar(); return send(Command::Visible, on); }
bool RadarService::pause(bool on) { if (on) sessions_.cancel_radar(); return send(Command::Pause, on); }
bool RadarService::select(uint32_t id) { return send(Command::Select, id); }
bool RadarService::filter(RadarFilter filter) { return send(Command::Filter, static_cast<uint32_t>(filter)); }
bool RadarService::sound(bool on) { if (!on) sessions_.cancel_radar(); return send(Command::Sound, on); }
RadarSnapshot RadarService::snapshot() const
{
    RadarSnapshot value;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        value = snapshot_; xSemaphoreGive(mutex_);
    }
    return value;
}
void RadarService::save_preferences()
{
    const auto value = model_.snapshot();
    nvs_handle_t nvs;
    esp_err_t result = nvs_open("radar", NVS_READWRITE, &nvs);
    if (result == ESP_OK) {
        result = nvs_set_u8(nvs, "version", 1);
        if (result == ESP_OK) result = nvs_set_u8(nvs, "filter", static_cast<uint8_t>(value.filter));
        if (result == ESP_OK) result = nvs_set_u8(nvs, "sound", value.sound);
        if (result == ESP_OK) result = nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (result != ESP_OK) ESP_LOGW("radar", "No se pudieron guardar preferencias: %s", esp_err_to_name(result));
}
void RadarService::task_entry(void* arg) { static_cast<RadarService*>(arg)->run(); }
void RadarService::run()
{
    uint32_t session = 0, last_batch = 0;
    int64_t next_wifi = 0, next_ble = 0, next_publish = 0, next_beep = 0;
    std::string wifi_status = "Detenido", ble_status = "Detenido";
    std::array<int16_t, 560> tone {};
    for (size_t i = 0; i < tone.size(); ++i) {
        const float envelope = std::min(1.f, static_cast<float>(std::min(i, tone.size()-i-1))/80.f);
        tone[i] = static_cast<int16_t>(2500.f * envelope * std::sin(6.2831853f * 1100.f * i / 16000.f));
    }
    while (true) {
        Message message;
        if (xQueueReceive(commands_, &message, pdMS_TO_TICKS(20)) == pdTRUE) {
            switch (message.command) {
                case Command::Visible:
                    if (message.value) model_.begin(); else model_.end();
                    break;
                case Command::Pause: model_.pause(message.value != 0); break;
                case Command::Select: model_.select(message.value); break;
                case Command::Filter: model_.filter(static_cast<RadarFilter>(message.value)); save_preferences(); break;
                case Command::Sound: model_.sound(message.value != 0); save_preferences(); break;
            }
            const auto state = model_.snapshot();
            const uint32_t next_session = state.active && !state.paused ? state.session : 0;
            if (session != next_session) {
                session = next_session;
                network_.radar_session(session);
                ble_.observe(0, ble_status);
                next_wifi = next_ble = next_beep = 0;
            }
            next_publish = 0;
        }
        const int64_t now = esp_timer_get_time()/1000;
        if (session) {
            if (now >= next_wifi) {
                if (network_.request_radar_scan(session)) wifi_status = "Buscando Wi-Fi";
                next_wifi = now + 5000;
            }
            const auto batch = network_.radar_wifi_batch(last_batch);
            if (batch.generation != last_batch && batch.session == session) {
                last_batch = batch.generation;
                for (const auto& o : batch.observations) model_.observe(session, o, now);
                wifi_status = batch.error.empty() ? "Wi-Fi actualizado" : batch.error;
                if (!batch.error.empty()) next_wifi = now + 2000;
            }
            if (now >= next_ble) {
                if (ble_.observe(session, ble_status)) ble_status = "Escuchando BLE";
                next_ble = now + 2000;
            }
            BleObservationReport report;
            for (int i = 0; i < 64 && ble_.read_observation(report); ++i) {
                RadarObservation o; o.kind = RadarKind::Ble; o.address = report.address;
                o.address_type = report.address_type; o.rssi = report.rssi;
                o.name = radar_ble_name(report.data.data(), report.length);
                model_.observe(report.session, o, now);
            }
        } else { wifi_status = "Detenido"; ble_status = "Detenido"; }
        model_.expire(now);
        if (now >= next_publish) {
            auto state = model_.snapshot();
            state.wifi_status = wifi_status; state.ble_status = ble_status;
            state.dropped_reports = ble_.dropped_reports();
            if (mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                snapshot_ = state; xSemaphoreGive(mutex_);
            }
            next_publish = now + 200;
            if (session && state.sound && state.selected_id && now >= next_beep && !network_.busy()) {
                const auto found = std::find_if(state.emitters.begin(), state.emitters.end(), [&](const RadarEmitter& e) { return e.id == state.selected_id; });
                if (found != state.emitters.end() && now-found->last_seen_ms < 15000) {
                    next_beep = now + static_cast<int64_t>(2000 - 1600 * radar_strength(found->smoothed_rssi));
                    AudioSessionLease lease(sessions_, AudioSessionOwner::Radar);
                    if (lease.acquired()) {
                        if (audio_.available() || audio_.init())
                            audio_.play_pcm(tone.data(), tone.size(), 16000, &sessions_.radar_cancel_requested());
                    }
                }
            }
        }
    }
}
} // namespace el_sistema
