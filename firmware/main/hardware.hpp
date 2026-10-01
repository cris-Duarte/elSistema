#pragma once

#include <cstdint>
#include <ctime>
#include <string>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace el_sistema {

class ClockService {
public:
    bool init(i2c_master_bus_handle_t bus);
    bool is_valid() const { return valid_; }
    int64_t now_utc() const;
    bool set_local(int year, int month, int day, int hour, int minute, int utc_offset_minutes);
    std::string format_local(int64_t utc, int utc_offset_minutes, bool include_time = true) const;

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
    void set_current(int rotation) { current_rotation_ = rotation; }
    bool available() const { return sensor_ != nullptr; }

private:
    void* sensor_ = nullptr;
    int current_rotation_ = 1;
    int candidate_rotation_ = -1;
    int64_t candidate_since_us_ = 0;
};

enum class ButtonEvent : uint8_t { None, ShortPress, LongPress };

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
