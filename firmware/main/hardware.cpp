#include "hardware.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sys/time.h>

#include "bmi270_api.h"
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

bool OrientationService::init(i2c_master_bus_handle_t bus)
{
    bmi270_handle_t sensor = nullptr;
    const esp_err_t err = bmi270_sensor_create_from_master_bus_with_address(
        bus, BMI270_I2C_ADDRESS, &sensor, bmi270_config_file,
        BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BMI270 no disponible: %s", esp_err_to_name(err));
        return false;
    }
    const uint8_t sensors[] = {BMI2_ACCEL};
    struct bmi2_sens_config config = {.type = BMI2_ACCEL};
    if (bmi2_set_adv_power_save(BMI2_DISABLE, sensor) != BMI2_OK ||
        bmi2_get_sensor_config(&config, 1, sensor) != BMI2_OK) {
        bmi270_sensor_del(&sensor);
        return false;
    }
    config.cfg.acc.odr = BMI2_ACC_ODR_25HZ;
    config.cfg.acc.range = BMI2_ACC_RANGE_2G;
    config.cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    config.cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    if (bmi2_set_sensor_config(&config, 1, sensor) != BMI2_OK || bmi2_sensor_enable(sensors, 1, sensor) != BMI2_OK) {
        bmi270_sensor_del(&sensor);
        return false;
    }
    sensor_ = sensor;
    ESP_LOGI(TAG, "Rotación automática lista");
    return true;
}

int OrientationService::poll()
{
    if (sensor_ == nullptr) return -1;
    struct bmi2_sens_data data {};
    if (bmi2_get_sensor_data(&data, static_cast<bmi270_handle_t>(sensor_)) != BMI2_OK ||
        !(data.status & BMI2_DRDY_ACC)) {
        return -1;
    }
    // El panel es nativo vertical (720x1280), asi que el eje Y del sensor recorre
    // su lado largo y el X el corto. En vertical la gravedad cae sobre el lado
    // largo (Y); en horizontal, sobre el corto (X). Tenerlo al reves hacia que la
    // pantalla se pusiera horizontal al sostener el Tab5 en vertical.
    constexpr int threshold = 7400;  // ~0.45 g at +/-2 g.
    const int x = data.acc.x;
    const int y = data.acc.y;
    int candidate = -1;
    if (std::abs(y) > std::abs(x) && std::abs(y) > threshold) {
        candidate = y > 0 ? 0 : 2;
    } else if (std::abs(x) > threshold) {
        // Verificado en el Tab5: la pareja horizontal estaba 180 grados al reves
        // (las dos posturas verticales ya eran correctas).
        candidate = x < 0 ? 3 : 1;
    }
    if (candidate < 0 || candidate == current_rotation_) {
        candidate_rotation_ = -1;
        candidate_since_us_ = 0;
        return -1;
    }
    const int64_t now = esp_timer_get_time();
    if (candidate != candidate_rotation_) {
        candidate_rotation_ = candidate;
        candidate_since_us_ = now;
        return -1;
    }
    if (now - candidate_since_us_ < 800000) return -1;
    current_rotation_ = candidate;
    candidate_rotation_ = -1;
    candidate_since_us_ = 0;
    // Registro con los valores crudos: sirve para verificar la orientacion real
    // del sensor en el Tab5 si alguna postura quedara del reves.
    ESP_LOGI(TAG, "Rotación automática: %d (acc x=%d y=%d)", current_rotation_, x, y);
    return current_rotation_;
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
