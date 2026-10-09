#pragma once

#include <cstdint>
#include <ctime>
#include <atomic>
#include <string>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "home_scene.hpp"
#include "device_state.hpp"

namespace el_sistema {

class ClockService {
public:
    bool init(i2c_master_bus_handle_t bus);
    bool is_valid() const { return valid_; }
    int64_t now_utc() const;
    bool set_local(int year, int month, int day, int hour, int minute, int utc_offset_minutes);
    std::string format_local(int64_t utc, int utc_offset_minutes, bool include_time = true) const;
    // Reloj compacto para la barra superior: "HH:MM DD/MM/AAAA".
    std::string format_status(int64_t utc, int utc_offset_minutes) const;

private:
    bool read_rtc(std::tm& value);
    bool write_rtc(const std::tm& value);

    i2c_master_dev_handle_t rtc_ = nullptr;
    bool valid_ = false;
};

class OrientationService {
public:
    bool init(i2c_master_bus_handle_t bus);
    int poll();
    void set_current(int rotation) { current_rotation_.store(rotation, std::memory_order_relaxed); }
    void set_motion_enabled(bool enabled) { motion_enabled_.store(enabled, std::memory_order_relaxed); }
    MotionSample motion_snapshot() const;
    bool available() const { return sensor_ != nullptr; }

private:
    static void task_entry(void* arg);
    void run();

    void* sensor_ = nullptr;
    QueueHandle_t motion_queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    std::atomic<int> current_rotation_{1};
    // Stable detected state survives UI locks; it is not a one-shot event.
    std::atomic<int> detected_rotation_{1};
    std::atomic<bool> motion_enabled_{false};
    std::atomic<bool> gyro_available_{false};
};

enum class ButtonEvent : uint8_t { None, ShortPress, LongPress };

enum class BatteryState : uint8_t { Unavailable, Missing, Available };

// Missing is a successful reading without a pack; Unavailable is a failed
// reading. Never turn a communication failure into a fabricated USB/0% state.
struct PowerStatus {
    BatteryState state = BatteryState::Unavailable;
    bool charging = false;  // CHG_STAT indicates charging, not USB presence.
    int battery_mv = -1;
    int battery_pct = -1;
};

// Energía del Tab5. El monitor INA226 (I2C 0x41) mide el bus de batería
// (NP-F550 extraíble de dos celdas) y el estado de carga sale del pin CHG_STAT
// del expansor E2. Fórmula de nivel de M5Unified: por celda 3300–4100 mV.
class PowerService {
public:
    bool init(i2c_master_bus_handle_t bus);
    PowerStatus poll() const;
    bool available() const { return monitor_ != nullptr && status_queue_ != nullptr; }

private:
    static void task_entry(void* arg);
    void run();
    PowerStatus read_status();
    i2c_master_dev_handle_t monitor_ = nullptr;
    QueueHandle_t status_queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
};

// Boton fisico del Tab5. El unico legible por software es el boton BOOT
// (GPIO35): el boton de encendido es un latch de hardware que el ESP32-P4 no
// puede leer, solo pedir el apagado con bsp_generate_poweroff_signal().
class ButtonService {
public:
    bool init(i2c_master_bus_handle_t bus);
    ButtonEvent next_event();

private:
    static void task_entry(void* arg);
    void run();

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;
    // Sonda temporal de las entradas de los expansores PI4IOE (solo lectura).
    i2c_master_dev_handle_t expander1_ = nullptr;
    i2c_master_dev_handle_t expander2_ = nullptr;
    uint8_t last_expander1_input_ = 0xFF;
    uint8_t last_expander2_input_ = 0xFF;
};

}  // namespace el_sistema
