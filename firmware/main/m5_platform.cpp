#include "m5_platform.hpp"

#include "M5Unified.h"
#include "bsp/m5stack_tab5.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {
StaticSemaphore_t bus_mutex_storage;
SemaphoreHandle_t bus_mutex = nullptr;

// Declaring the already-owned display prevents M5.begin() from opening a
// second DSI panel or resetting the BSP's touch/rotation configuration.
class BspOwnedDisplay : public M5GFX {
public:
    BspOwnedDisplay() { _board = m5gfx::board_t::board_M5Tab5; setPanel(&panel_); }
private:
    lgfx::Panel_NULL panel_;
};
BspOwnedDisplay display_facade;
}  // namespace

namespace el_sistema {
HardwareI2cGuard::HardwareI2cGuard()
{
    if (bus_mutex != nullptr) xSemaphoreTakeRecursive(bus_mutex, portMAX_DELAY);
}
HardwareI2cGuard::~HardwareI2cGuard()
{
    if (bus_mutex != nullptr) xSemaphoreGiveRecursive(bus_mutex);
}

bool init_m5_platform()
{
    bus_mutex = xSemaphoreCreateRecursiveMutexStatic(&bus_mutex_storage);
    if (bus_mutex == nullptr) return false;
    HardwareI2cGuard guard;
    M5.Display = display_facade;
    auto config = M5.config();
    config.clear_display = false;
    config.internal_mic = false;
    config.internal_spk = false;
    config.internal_rtc = false;
    config.disable_rtc_irq = false;
    config.external_imu = false;
    config.external_rtc = false;
    config.external_display_value = 0;
    config.output_power = false;
    config.fallback_board = m5::board_t::board_M5Tab5;
    M5.begin(config);

    // M5Unified boots Tab5 on I2C1. Before starting any workers, release it
    // and bind all its internal devices to the BSP's shared native I2C0 bus.
    M5.In_I2C.release();
    if (bsp_i2c_init() != ESP_OK ||
        !M5.In_I2C.begin(static_cast<i2c_port_t>(BSP_I2C_NUM), BSP_I2C_SDA, BSP_I2C_SCL)) return false;
    ESP_LOGI("m5_platform", "M5Unified listo; display y audio bajo BSP, I2C compartido protegido");
    return true;
}
}  // namespace el_sistema

// M5GFX uses register-level I2C, which must never overlap native IDF transfers.
// Recursive locking also allows an M5 transaction to call an IDF API internally.
extern "C" {
esp_err_t __real_i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t*, size_t, int);
esp_err_t __real_i2c_master_receive(i2c_master_dev_handle_t, uint8_t*, size_t, int);
esp_err_t __real_i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t*, size_t, uint8_t*, size_t, int);
esp_err_t __real_i2c_master_probe(i2c_master_bus_handle_t, uint16_t, int);
esp_err_t __real_i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t, i2c_master_transmit_multi_buffer_info_t*, size_t, int);

esp_err_t __wrap_i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t* data, size_t size, int timeout)
{ el_sistema::HardwareI2cGuard guard; return __real_i2c_master_transmit(dev, data, size, timeout); }
esp_err_t __wrap_i2c_master_receive(i2c_master_dev_handle_t dev, uint8_t* data, size_t size, int timeout)
{ el_sistema::HardwareI2cGuard guard; return __real_i2c_master_receive(dev, data, size, timeout); }
esp_err_t __wrap_i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t* tx, size_t tx_size, uint8_t* rx, size_t rx_size, int timeout)
{ el_sistema::HardwareI2cGuard guard; return __real_i2c_master_transmit_receive(dev, tx, tx_size, rx, rx_size, timeout); }
esp_err_t __wrap_i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeout)
{ el_sistema::HardwareI2cGuard guard; return __real_i2c_master_probe(bus, address, timeout); }
esp_err_t __wrap_i2c_master_multi_buffer_transmit(i2c_master_dev_handle_t dev, i2c_master_transmit_multi_buffer_info_t* info, size_t size, int timeout)
{ el_sistema::HardwareI2cGuard guard; return __real_i2c_master_multi_buffer_transmit(dev, info, size, timeout); }
}
