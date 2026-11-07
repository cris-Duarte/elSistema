#pragma once
#include "esp_err.h"

namespace el_sistema {
// Application-wide owner. Video clients may come and go; this small event task
// is retained for the lifetime of the firmware (future HID clients can share it).
// Only the camera worker calls start(); never use the BSP's aborting USB helper.
class UsbHostRuntime {
public:
    static esp_err_t start();
private:
    static void event_task(void*);
};
}
