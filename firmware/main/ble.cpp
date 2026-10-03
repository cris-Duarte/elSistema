#include "ble.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "ble";
constexpr const char* kDeviceName = "El Sistema";

// UUIDs privados de El Sistema. Esta primera característica solo sirve para
// leer el estado y escribir "ping" desde una app de diagnóstico BLE.
const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x10, 0x7a, 0x65, 0x6d, 0x61, 0x74, 0x73, 0x69,
    0x73, 0x2d, 0x65, 0x6c, 0x00, 0x00, 0x00, 0x01);
const ble_uuid128_t kDiagnosticUuid = BLE_UUID128_INIT(
    0x10, 0x7a, 0x65, 0x6d, 0x61, 0x74, 0x73, 0x69,
    0x73, 0x2d, 0x65, 0x6c, 0x00, 0x00, 0x00, 0x02);

}  // namespace

BleService* BleService::instance_ = nullptr;

const char* ble_phase_name(BlePhase phase)
{
    switch (phase) {
        case BlePhase::Disabled: return "DESACTIVADO";
        case BlePhase::Starting: return "INICIANDO";
        case BlePhase::Advertising: return "ANUNCIANDO";
        case BlePhase::Connected: return "CONECTADO";
        case BlePhase::Error: return "ERROR";
    }
    return "ERROR";
}

BleSnapshot BleService::snapshot() const
{
    BleSnapshot result;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(30)) == pdTRUE) {
        result = snapshot_;
        xSemaphoreGive(mutex_);
    }
    return result;
}

void BleService::set_state(BlePhase phase, const std::string& message, const std::string& error)
{
    if (mutex_ == nullptr) return;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_.phase = phase;
        snapshot_.message = message;
        snapshot_.error = error;
        snapshot_.connected = phase == BlePhase::Connected;
        snapshot_.free_internal_bytes = static_cast<uint32_t>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        snapshot_.minimum_internal_bytes = static_cast<uint32_t>(
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        ++snapshot_.generation;
        xSemaphoreGive(mutex_);
    }
}

bool BleService::register_gatt()
{
    static const ble_gatt_chr_def characteristics[] = {
        {
            .uuid = &kDiagnosticUuid.u,
            .access_cb = gatt_access,
            .arg = nullptr,
            .descriptors = nullptr,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            .min_key_size = 0,
            .val_handle = nullptr,
        },
        {},
    };
    static const ble_gatt_svc_def services[] = {
        {
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = &kServiceUuid.u,
            .includes = nullptr,
            .characteristics = characteristics,
        },
        {},
    };

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int result = ble_gatts_count_cfg(services);
    if (result == 0) result = ble_gatts_add_svcs(services);
    if (result != 0) {
        set_state(BlePhase::Error, "No se pudo registrar el servicio BLE", "GATT rc=" + std::to_string(result));
        return false;
    }
    return true;
}

bool BleService::init()
{
    if (initialized_) return true;
    if (mutex_ == nullptr) mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) return false;
    set_state(BlePhase::Starting, "Iniciando NimBLE sobre el ESP32-C6");
    instance_ = this;

    const esp_err_t result = nimble_port_init();
    if (result != ESP_OK) {
        set_state(BlePhase::Error, "No se pudo iniciar Bluetooth", esp_err_to_name(result));
        return false;
    }
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 0;
    if (!register_gatt()) return false;

    const int name_result = ble_svc_gap_device_name_set(kDeviceName);
    if (name_result != 0) {
        set_state(BlePhase::Error, "No se pudo asignar el nombre BLE", "GAP rc=" + std::to_string(name_result));
        return false;
    }
    initialized_ = true;
    nimble_port_freertos_init(host_task);
    ESP_LOGI(TAG, "NimBLE iniciado; esperando sincronización con el ESP32-C6");
    return true;
}

void BleService::host_task(void*)
{
    ESP_LOGI(TAG, "Tarea NimBLE iniciada");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void BleService::on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE se reinició: %d", reason);
    if (instance_ != nullptr)
        instance_->set_state(BlePhase::Error, "Bluetooth perdió la conexión con el C6",
                             "NimBLE reset " + std::to_string(reason));
}

void BleService::on_sync()
{
    if (instance_ == nullptr) return;
    const int address_result = ble_hs_util_ensure_addr(0);
    if (address_result != 0 || ble_hs_id_infer_auto(0, &instance_->address_type_) != 0) {
        instance_->set_state(BlePhase::Error, "No se pudo obtener la identidad BLE");
        return;
    }
    instance_->advertise();
}

bool BleService::advertise()
{
    if (ble_gap_adv_active()) return true;
    ble_hs_adv_fields fields {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    int result = ble_gap_adv_set_fields(&fields);

    ble_hs_adv_fields response {};
    response.name = reinterpret_cast<uint8_t*>(const_cast<char*>(kDeviceName));
    response.name_len = std::strlen(kDeviceName);
    response.name_is_complete = 1;
    if (result == 0) result = ble_gap_adv_rsp_set_fields(&response);

    ble_gap_adv_params parameters {};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    if (result == 0)
        result = ble_gap_adv_start(address_type_, nullptr, BLE_HS_FOREVER, &parameters, gap_event, this);
    if (result != 0) {
        set_state(BlePhase::Error, "No se pudo anunciar El Sistema", "GAP rc=" + std::to_string(result));
        return false;
    }
    set_state(BlePhase::Advertising, "Visible como “El Sistema”");
    ESP_LOGI(TAG, "Anuncio BLE activo: %s", kDeviceName);
    return true;
}

int BleService::gap_event(ble_gap_event* event, void* argument)
{
    auto* service = static_cast<BleService*>(argument);
    if (service == nullptr) service = instance_;
    if (service == nullptr) return 0;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                service->set_state(BlePhase::Connected, "Dispositivo BLE conectado");
                ESP_LOGI(TAG, "Cliente BLE conectado");
            } else {
                ESP_LOGW(TAG, "Conexión BLE fallida: %d", event->connect.status);
                service->advertise();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Cliente BLE desconectado: %d", event->disconnect.reason);
            service->set_state(BlePhase::Advertising, "Cliente desconectado; anunciando nuevamente");
            service->advertise();
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            service->advertise();
            break;
        default:
            break;
    }
    return 0;
}

std::string BleService::diagnostic_payload() const
{
    const BleSnapshot value = snapshot();
    char payload[176];
    std::snprintf(payload, sizeof(payload),
                  "{\"name\":\"El Sistema\",\"protocol\":1,\"state\":\"%s\",\"probes\":%lu,"
                  "\"heap_free\":%lu,\"heap_min\":%lu}",
                  ble_phase_name(value.phase), static_cast<unsigned long>(value.probe_count),
                  static_cast<unsigned long>(value.free_internal_bytes),
                  static_cast<unsigned long>(value.minimum_internal_bytes));
    return payload;
}

int BleService::gatt_access(uint16_t, uint16_t, ble_gatt_access_ctxt* context, void*)
{
    if (instance_ == nullptr) return BLE_ATT_ERR_UNLIKELY;
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const std::string payload = instance_->diagnostic_payload();
        return os_mbuf_append(context->om, payload.data(), payload.size()) == 0
                   ? 0
                   : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        constexpr size_t kMaximumProbeLength = 64;
        const uint16_t length = OS_MBUF_PKTLEN(context->om);
        if (length == 0 || length > kMaximumProbeLength) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        std::array<char, kMaximumProbeLength + 1> input {};
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(context->om, input.data(), kMaximumProbeLength, &copied) != 0)
            return BLE_ATT_ERR_UNLIKELY;
        if (instance_->mutex_ != nullptr && xSemaphoreTake(instance_->mutex_, portMAX_DELAY) == pdTRUE) {
            ++instance_->snapshot_.probe_count;
            instance_->snapshot_.free_internal_bytes = static_cast<uint32_t>(
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            instance_->snapshot_.minimum_internal_bytes = static_cast<uint32_t>(
                heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            ++instance_->snapshot_.generation;
            xSemaphoreGive(instance_->mutex_);
        }
        ESP_LOGI(TAG, "Prueba GATT recibida (%u bytes)%s", static_cast<unsigned>(copied),
                 std::strncmp(input.data(), "ping", 4) == 0 ? ": ping" : "");
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

}  // namespace el_sistema
