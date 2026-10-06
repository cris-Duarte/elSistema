#include "hardware.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sys/time.h>

#include "M5Unified.h"
#include "m5_platform.hpp"
#include "bsp/m5stack_tab5.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "hardware";
constexpr uint8_t kRtcAddress = 0x32;
constexpr uint8_t kRtcTimeRegister = 0x10;
constexpr uint8_t kRtcFlagRegister = 0x1D;
constexpr int kI2cTimeoutMs = 200;
constexpr int64_t kMinimumValidEpoch = 1704067200;  // 2024-01-01 UTC

uint8_t bcd_to_byte(uint8_t value)
{
    return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

uint8_t byte_to_bcd(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

bool valid_bcd(uint8_t value)
{
    return (value & 0x0F) <= 9 && ((value >> 4) & 0x0F) <= 9;
}

time_t utc_from_tm(std::tm& value)
{
    setenv("TZ", "UTC0", 1);
    tzset();
    return mktime(&value);
}

}  // namespace

bool ClockService::init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kRtcAddress,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &config, &rtc_) != ESP_OK) {
        ESP_LOGW(TAG, "RTC RX8130CE no disponible");
        rtc_ = nullptr;
        return false;
    }

    std::tm rtc_time {};
    valid_ = read_rtc(rtc_time);
    if (valid_) {
        const time_t epoch = utc_from_tm(rtc_time);
        if (epoch >= kMinimumValidEpoch) {
            const timeval value = {.tv_sec = epoch, .tv_usec = 0};
            settimeofday(&value, nullptr);
            ESP_LOGI(TAG, "Reloj del sistema restaurado desde RX8130CE");
        } else {
            valid_ = false;
        }
    }
    return true;
}

bool ClockService::read_rtc(std::tm& value)
{
    if (rtc_ == nullptr) return false;
    uint8_t start = kRtcTimeRegister;
    uint8_t data[7] = {};
    if (i2c_master_transmit_receive(rtc_, &start, 1, data, sizeof(data), kI2cTimeoutMs) != ESP_OK) return false;
    for (int i : {0, 1, 2, 4, 5, 6}) {
        if (!valid_bcd(data[i] & (i == 2 ? 0x3F : i == 4 ? 0x3F : i == 5 ? 0x1F : 0x7F))) return false;
    }
    const int second = bcd_to_byte(data[0] & 0x7F);
    const int minute = bcd_to_byte(data[1] & 0x7F);
    const int hour = bcd_to_byte(data[2] & 0x3F);
    const int day = bcd_to_byte(data[4] & 0x3F);
    const int month = bcd_to_byte(data[5] & 0x1F);
    const int year = 2000 + bcd_to_byte(data[6]);
    if (second > 59 || minute > 59 || hour > 23 || day < 1 || day > 31 || month < 1 || month > 12 || year < 2024) {
        return false;
    }
    value.tm_sec = second;
    value.tm_min = minute;
    value.tm_hour = hour;
    value.tm_mday = day;
    value.tm_mon = month - 1;
    value.tm_year = year - 1900;
    value.tm_isdst = 0;

    uint8_t flag_reg = kRtcFlagRegister;
    uint8_t flags = 0;
    if (i2c_master_transmit_receive(rtc_, &flag_reg, 1, &flags, 1, kI2cTimeoutMs) == ESP_OK && (flags & 0x02)) {
        return false;
    }
    return true;
}

bool ClockService::write_rtc(const std::tm& value)
{
    if (rtc_ == nullptr) return false;
    uint8_t data[8] = {
        kRtcTimeRegister,
        byte_to_bcd(static_cast<uint8_t>(value.tm_sec)),
        byte_to_bcd(static_cast<uint8_t>(value.tm_min)),
        byte_to_bcd(static_cast<uint8_t>(value.tm_hour)),
        static_cast<uint8_t>(1u << std::clamp(value.tm_wday, 0, 6)),
        byte_to_bcd(static_cast<uint8_t>(value.tm_mday)),
        byte_to_bcd(static_cast<uint8_t>(value.tm_mon + 1)),
        byte_to_bcd(static_cast<uint8_t>((value.tm_year + 1900) % 100)),
    };
    if (i2c_master_transmit(rtc_, data, sizeof(data), kI2cTimeoutMs) != ESP_OK) return false;
    uint8_t clear_flags[2] = {kRtcFlagRegister, 0x00};
    i2c_master_transmit(rtc_, clear_flags, sizeof(clear_flags), kI2cTimeoutMs);
    return true;
}

int64_t ClockService::now_utc() const
{
    const time_t now = time(nullptr);
    if (now >= kMinimumValidEpoch) return static_cast<int64_t>(now);
    return kMinimumValidEpoch + esp_timer_get_time() / 1000000;
}

bool ClockService::set_local(int year, int month, int day, int hour, int minute, int utc_offset_minutes)
{
    std::tm local {};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_isdst = 0;
    time_t epoch = utc_from_tm(local);
    if (epoch < kMinimumValidEpoch) return false;
    epoch -= utc_offset_minutes * 60;
    std::tm utc {};
    gmtime_r(&epoch, &utc);
    if (!write_rtc(utc)) return false;
    const timeval value = {.tv_sec = epoch, .tv_usec = 0};
    settimeofday(&value, nullptr);
    valid_ = true;
    return true;
}

std::string ClockService::format_local(int64_t utc, int utc_offset_minutes, bool include_time) const
{
    if (utc <= 0) return "Sin fecha";
    time_t local_epoch = static_cast<time_t>(utc + utc_offset_minutes * 60);
    std::tm value {};
    gmtime_r(&local_epoch, &value);
    char output[32];
    std::strftime(output, sizeof(output), include_time ? "%d/%m/%Y %H:%M" : "%d/%m/%Y", &value);
    return output;
}

std::string ClockService::format_status(int64_t utc, int utc_offset_minutes) const
{
    if (utc <= 0) return "--:--";
    time_t local_epoch = static_cast<time_t>(utc + utc_offset_minutes * 60);
    std::tm value {};
    gmtime_r(&local_epoch, &value);
    char output[24];
    std::strftime(output, sizeof(output), "%H:%M %d/%m/%Y", &value);
    return output;
}

bool OrientationService::init(i2c_master_bus_handle_t bus)
{
    (void)bus;  // M5Unified owns the IMU on the protected shared BSP bus.
    HardwareI2cGuard guard;
    if (!M5.Imu.isEnabled() || M5.Imu.getType() != m5::imu_t::imu_bmi270) {
        ESP_LOGW(TAG, "BMI270 no disponible en M5Unified");
        return false;
    }
    // BMI270: 100 Hz, ±8 g / ±2000 dps, normal filtering. Match M5Unified's
    // conversion factors and leave enough headroom for the 2.5 g shake gesture.
    constexpr uint8_t address = 0x68;
    constexpr uint32_t frequency = 400000;
    if (!M5.In_I2C.writeRegister8(address, 0x40, 0xA8, frequency) ||
        !M5.In_I2C.writeRegister8(address, 0x41, 0x02, frequency) ||
        !M5.In_I2C.writeRegister8(address, 0x42, 0xA8, frequency) ||
        !M5.In_I2C.writeRegister8(address, 0x43, 0x00, frequency) ||
        !M5.In_I2C.writeRegister8(address, 0x7D, 0x0C, frequency)) {
        ESP_LOGW(TAG, "No se pudo configurar BMI270");
        return false;
    }
    // M5.begin() may load offsets (or estimate them before the range is set).
    // They must not be applied with a different raw scale: retain gravity and
    // use uncalibrated ±8 g readings for this visual gesture, without erasing NVS.
    M5.Imu.setCalibration(0, 0, 0);
    M5.Imu.clearOffsetData();
    sensor_ = &M5.Imu;
    motion_queue_ = xQueueCreate(1, sizeof(MotionSample));
    if (motion_queue_ == nullptr || xTaskCreate(task_entry, "orientation", 4096, this, 3, &task_) != pdPASS) {
        if (motion_queue_ != nullptr) vQueueDelete(motion_queue_);
        motion_queue_ = nullptr;
        sensor_ = nullptr;
        ESP_LOGW(TAG, "No se pudo crear la tarea de movimiento BMI270");
        return false;
    }
    ESP_LOGI(TAG, "BMI270 M5Unified listo: ±8 g, muestras 50 Hz, giroscopio solo en Inicio");
    return true;
}

int OrientationService::poll()
{
    return available() ? detected_rotation_.load(std::memory_order_relaxed) : -1;
}

MotionSample OrientationService::motion_snapshot() const
{
    MotionSample sample{};
    if (motion_queue_ != nullptr) xQueuePeek(motion_queue_, &sample, 0);
    return sample;
}

void OrientationService::task_entry(void* arg)
{
    static_cast<OrientationService*>(arg)->run();
}

void OrientationService::run()
{
    bool gyro_enabled = false;
    bool motion_stale = false;
    int64_t last_gyro_toggle_attempt_us = 0;
    int64_t last_motion_sample_us = 0;
    int64_t last_repair_us = 0;
    OrientationDetector detector(current_rotation_.load());
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        const bool should_enable_gyro = home_motion_enabled_.load(std::memory_order_relaxed);
        const int64_t loop_now_us = esp_timer_get_time();
        const bool gyro_retry_due = last_gyro_toggle_attempt_us == 0 ||
                                    loop_now_us - last_gyro_toggle_attempt_us >= 1000000;
        if (should_enable_gyro != gyro_enabled && (!should_enable_gyro || gyro_retry_due)) {
            last_gyro_toggle_attempt_us = loop_now_us;
            HardwareI2cGuard guard;
            const bool result = M5.In_I2C.writeRegister8(0x68, 0x7D, should_enable_gyro ? 0x0E : 0x0C, 400000);
            gyro_enabled = result ? should_enable_gyro : gyro_enabled;
            gyro_available_.store(gyro_enabled, std::memory_order_relaxed);
            if (!result) ESP_LOGW(TAG, "No se pudo %s el giroscopio BMI270", should_enable_gyro ? "activar" : "desactivar");
        }

        MotionSample sample{};
        bool read_ok;
        {
            HardwareI2cGuard guard;
            const auto mask = M5.Imu.update();
            read_ok = (mask & m5::IMU_Class::sensor_mask_accel) != 0;
            if (read_ok) {
                M5.Imu.getAccel(&sample.accel_x, &sample.accel_y, &sample.accel_z);
                sample.gyro_available = gyro_enabled && (mask & m5::IMU_Class::sensor_mask_gyro);
                if (sample.gyro_available) M5.Imu.getGyro(&sample.gyro_x, &sample.gyro_y, &sample.gyro_z);
                sample.valid = true;
            }
        }
        // M5's BMI270 driver gates reads on INT_STATUS_1. If that mapping is
        // lost, restore it and the acquisition registers after a stale period.
        // Do not reset the chip, touch, calibration storage or the shared bus.
        if (!read_ok && loop_now_us - last_motion_sample_us >= 500000 &&
            loop_now_us - last_repair_us >= 2000000) {
            last_repair_us = loop_now_us;
            HardwareI2cGuard guard;
            const bool restored = M5.In_I2C.writeRegister8(0x68, 0x58, 0xFF, 400000) &&
                                  M5.In_I2C.writeRegister8(0x68, 0x40, 0xA8, 400000) &&
                                  M5.In_I2C.writeRegister8(0x68, 0x41, 0x02, 400000) &&
                                  M5.In_I2C.writeRegister8(0x68, 0x7D, gyro_enabled ? 0x0E : 0x0C, 400000);
            ESP_LOGW(TAG, "BMI270 sin muestras: recuperación %s", restored ? "solicitada" : "fallida");
        }
        if (read_ok) {
            last_motion_sample_us = esp_timer_get_time();
            motion_stale = false;
            if (motion_queue_ != nullptr) xQueueOverwrite(motion_queue_, &sample);

            const int previous = detected_rotation_.load(std::memory_order_relaxed);
            const int detected = detector.update(sample, esp_timer_get_time());
            if (detected != previous) {
                detected_rotation_.store(detected, std::memory_order_relaxed);
                ESP_LOGI(TAG, "Rotación detectada: %d (acc x=%.2f y=%.2f g)", detected, sample.accel_x, sample.accel_y);
            }
        } else if (!motion_stale && last_motion_sample_us != 0 &&
                   esp_timer_get_time() - last_motion_sample_us >= 160000) {
            // Si el acelerómetro deja de publicar muestras, no mantengas una
            // inclinación vieja indefinidamente: la escena vuelve a gravedad
            // vertical hasta recuperar lecturas válidas.
            MotionSample unavailable{};
            if (motion_queue_ != nullptr) xQueueOverwrite(motion_queue_, &unavailable);
            detector.update(unavailable, esp_timer_get_time());
            ESP_LOGW(TAG, "Muestras BMI270 caducadas; orientación conservada");
            motion_stale = true;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(home_motion_enabled_.load() ? HOME_FRAME_MS : 40));
    }
}

namespace {

// Monitor de energía INA226: vive en el mismo bus I2C que el RTC y el
// sensor de orientación. Solo se usa el registro de voltaje de bus (0x02),
// que no necesita calibración: bits 14..0 con LSB de 1,25 mV. El bus es la
// batería tal cual (fórmula de M5Unified: por celda 3300–4100 mV).
constexpr uint8_t kPowerMonitorAddress = 0x41;
constexpr uint8_t kPowerConfigRegister = 0x00;
constexpr uint8_t kPowerBusVoltageRegister = 0x02;
// Promedio de 16 muestras, conversión de 1100 µs en ambos canales y medida
// continua de shunt+bus, como configura M5Unified para el Tab5.
constexpr uint8_t kPowerConfigMsb = 0x53;
constexpr uint8_t kPowerConfigLsb = 0x27;
// Por debajo de esto no hay batería instalada (el pin flota); el equipo solo
// puede estar corriendo por USB.
constexpr int kBatteryPresentMv = 5000;

}  // namespace

bool PowerService::init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kPowerMonitorAddress,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &config, &monitor_) != ESP_OK) {
        ESP_LOGW(TAG, "Monitor de energía INA226 no disponible");
        monitor_ = nullptr;
        return false;
    }
    const uint8_t setup[3] = {kPowerConfigRegister, kPowerConfigMsb, kPowerConfigLsb};
    if (i2c_master_transmit(monitor_, setup, sizeof(setup), kI2cTimeoutMs) != ESP_OK) {
        ESP_LOGW(TAG, "INA226 no aceptó la configuración; se usa la de arranque");
    }
    ESP_LOGI(TAG, "Monitor de energía listo (INA226)");
    status_queue_ = xQueueCreate(1, sizeof(PowerStatus));
    if (status_queue_ == nullptr || xTaskCreate(task_entry, "power_monitor", 3072, this, 2, &task_) != pdPASS) {
        if (status_queue_ != nullptr) vQueueDelete(status_queue_);
        status_queue_ = nullptr;
        ESP_LOGW(TAG, "No se pudo crear la tarea de energía");
        return false;
    }
    return true;
}

PowerStatus PowerService::poll() const
{
    PowerStatus status;
    if (status_queue_ != nullptr) xQueuePeek(status_queue_, &status, 0);
    return status;
}

void PowerService::task_entry(void* arg) { static_cast<PowerService*>(arg)->run(); }

void PowerService::run()
{
    while (true) {
        const PowerStatus status = read_status();
        xQueueOverwrite(status_queue_, &status);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

PowerStatus PowerService::read_status()
{
    PowerStatus status;
    // El BSP lo llama "usb_c_detect", pero el bit 6 de E2 es CHG_STAT del
    // IP2326: alto mientras carga, bajo con batería llena o descargando.
    status.charging = bsp_usb_c_detect();
    if (monitor_ == nullptr) return status;
    const uint8_t reg = kPowerBusVoltageRegister;
    uint8_t data[2] = {};
    if (i2c_master_transmit_receive(monitor_, &reg, 1, data, sizeof(data), kI2cTimeoutMs) != ESP_OK) {
        return status;
    }
    const uint16_t raw = static_cast<uint16_t>((data[0] << 8) | data[1]);
    const int pack_mv = ina226_battery_mv(raw);
    if (pack_mv < 0 || pack_mv > 9000) return status;
    status.state = pack_mv < kBatteryPresentMv ? BatteryState::Missing : BatteryState::Available;
    if (status.state == BatteryState::Missing) return status;
    status.battery_mv = pack_mv;
    status.battery_pct = battery_percent_from_mv(pack_mv);
    static bool logged_once = false;
    if (!logged_once) {
        logged_once = true;
        ESP_LOGI(TAG, "Batería: %d mV -> %d%% (%s)", status.battery_mv, status.battery_pct,
                 status.charging ? "cargando" : "sin cargar");
    }
    return status;
}

namespace {

constexpr gpio_num_t kButtonPin = GPIO_NUM_35;  // boton BOOT del Tab5
constexpr int kButtonPollMs = 15;
constexpr int kButtonDebounceMs = 25;
constexpr int64_t kLongPressUs = 2000000;  // 2 s

// Sonda temporal: el boton visible del Tab5 no es GPIO35, asi que se vigilan las
// entradas de los dos expansores PI4IOE (solo lectura, sin reconfigurar nada)
// para ver si alguna reacciona a la pulsacion.
constexpr uint8_t kExpander1Address = 0x43;
constexpr uint8_t kExpander2Address = 0x44;
constexpr uint8_t kExpanderInputRegister = 0x0F;
constexpr int kProbeEveryLoops = 6;

uint8_t read_expander_input(i2c_master_dev_handle_t device)
{
    if (device == nullptr) return 0xFF;
    const uint8_t reg = kExpanderInputRegister;
    uint8_t value = 0xFF;
    if (i2c_master_transmit_receive(device, &reg, 1, &value, 1, 100) != ESP_OK) return 0xFF;
    return value;
}

}  // namespace

bool ButtonService::init(i2c_master_bus_handle_t bus)
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << static_cast<uint32_t>(kButtonPin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo configurar el boton fisico");
        return false;
    }
    queue_ = xQueueCreate(4, sizeof(ButtonEvent));
    if (queue_ == nullptr) return false;
    if (xTaskCreate(task_entry, "boton_fisico", 3072, this, 4, &task_) != pdPASS) {
        ESP_LOGW(TAG, "No se pudo crear la tarea del boton fisico");
        vQueueDelete(queue_);
        queue_ = nullptr;
        return false;
    }

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.scl_speed_hz = 400000;
    device_config.device_address = kExpander1Address;
    if (i2c_master_bus_add_device(bus, &device_config, &expander1_) != ESP_OK) expander1_ = nullptr;
    device_config.device_address = kExpander2Address;
    if (i2c_master_bus_add_device(bus, &device_config, &expander2_) != ESP_OK) expander2_ = nullptr;
    last_expander1_input_ = read_expander_input(expander1_);
    last_expander2_input_ = read_expander_input(expander2_);

    ESP_LOGI(TAG, "Boton fisico listo (GPIO%d); sonda expansores 0x43=0x%02X 0x44=0x%02X",
             static_cast<int>(kButtonPin), last_expander1_input_, last_expander2_input_);
    return true;
}

void ButtonService::task_entry(void* arg)
{
    static_cast<ButtonService*>(arg)->run();
}

void ButtonService::run()
{
    bool released = gpio_get_level(kButtonPin) != 0;
    bool long_reported = false;
    int64_t pressed_at_us = 0;
    int probe_loops = 0;
    while (true) {
        const bool current = gpio_get_level(kButtonPin) != 0;
        if (current != released) {
            vTaskDelay(pdMS_TO_TICKS(kButtonDebounceMs));
            if ((gpio_get_level(kButtonPin) != 0) == current) {
                released = current;
                if (released) {
                    // Solo cuenta como pulsacion corta si no se avisó ya de la larga.
                    if (!long_reported) {
                        const ButtonEvent event = ButtonEvent::ShortPress;
                        xQueueSend(queue_, &event, 0);
                    }
                    long_reported = false;
                } else {
                    pressed_at_us = esp_timer_get_time();
                    long_reported = false;
                }
            }
        }
        if (!released && !long_reported && esp_timer_get_time() - pressed_at_us >= kLongPressUs) {
            long_reported = true;
            const ButtonEvent event = ButtonEvent::LongPress;
            xQueueSend(queue_, &event, 0);
        }
        if (++probe_loops >= kProbeEveryLoops) {
            probe_loops = 0;
            const uint8_t input1 = read_expander_input(expander1_);
            if (input1 != last_expander1_input_) {
                ESP_LOGW(TAG, "Sonda: expansor 0x43 IN 0x%02X -> 0x%02X", last_expander1_input_, input1);
                last_expander1_input_ = input1;
            }
            const uint8_t input2 = read_expander_input(expander2_);
            if (input2 != last_expander2_input_) {
                ESP_LOGW(TAG, "Sonda: expansor 0x44 IN 0x%02X -> 0x%02X", last_expander2_input_, input2);
                last_expander2_input_ = input2;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kButtonPollMs));
    }
}

ButtonEvent ButtonService::next_event()
{
    ButtonEvent event = ButtonEvent::None;
    if (queue_ != nullptr) xQueueReceive(queue_, &event, 0);
    return event;
}

}  // namespace el_sistema
