#pragma once

namespace el_sistema {

// The BSP remains the only physical display/touch/audio owner. M5Unified
// initializes the board and IMU; M5Canvas renders detached RAM frames.
bool init_m5_platform();

class HardwareI2cGuard {
public:
    HardwareI2cGuard();
    ~HardwareI2cGuard();
    HardwareI2cGuard(const HardwareI2cGuard&) = delete;
    HardwareI2cGuard& operator=(const HardwareI2cGuard&) = delete;
};

}  // namespace el_sistema
