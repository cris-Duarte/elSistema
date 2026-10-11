#pragma once

#include <cstdint>
#include <string>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct ble_gap_event;
struct ble_gatt_access_ctxt;

namespace el_sistema {

enum class BlePhase : uint8_t {
    Disabled,
    Starting,
    Advertising,
    Connected,
    Error,
};

struct BleSnapshot {
    BlePhase phase = BlePhase::Disabled;
    std::string message = "BLE desactivado";
    std::string error;
    uint32_t generation = 0;
    uint32_t probe_count = 0;
    uint32_t free_internal_bytes = 0;
    uint32_t minimum_internal_bytes = 0;
    bool connected = false;
};

const char* ble_phase_name(BlePhase phase);

// Servicio GATT minimo para validar NimBLE sobre ESP-Hosted/VHCI. No recibe
// contraseñas ni controla misiones: esas capacidades solo se añadirán después
// de comprobar coexistencia real entre Wi-Fi y BLE.
class BleService {
public:
    bool init();
    BleSnapshot snapshot() const;

private:
    static void host_task(void* argument);
    static void on_reset(int reason);
    static void on_sync();
    static int gap_event(ble_gap_event* event, void* argument);
    static int gatt_access(uint16_t connection_handle, uint16_t attribute_handle,
                           ble_gatt_access_ctxt* context, void* argument);

    bool register_gatt();
    bool advertise();
    std::string diagnostic_payload() const;
    void set_state(BlePhase phase, const std::string& message, const std::string& error = {});

    static BleService* instance_;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    BleSnapshot snapshot_;
    bool initialized_ = false;
    uint8_t address_type_ = 0;
};

}  // namespace el_sistema
