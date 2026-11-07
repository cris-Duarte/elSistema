#include "usb_host_runtime.hpp"
#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

namespace el_sistema {
esp_err_t UsbHostRuntime::start()
{
    // A single service worker invokes this; no competing install/uninstall path.
    static bool ready = false;
    if (ready) return ESP_OK;
    usb_host_config_t config{};
    config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    esp_err_t err = usb_host_install(&config);
    if (err != ESP_OK) return err; // Do not assume ownership of somebody else's host.
    if (xTaskCreate(event_task, "usb-host", 4096, nullptr, 5, nullptr) != pdPASS) {
        usb_host_uninstall(); return ESP_ERR_NO_MEM;
    }
    ready = true;
    return ESP_OK;
}
void UsbHostRuntime::event_task(void*)
{
    while (true) {
        uint32_t flags = 0;
        const auto err = usb_host_lib_handle_events(pdMS_TO_TICKS(100), &flags);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGW("usb-host", "Eventos USB: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            const auto freed = usb_host_device_free_all();
            if (freed != ESP_OK && freed != ESP_ERR_NOT_FINISHED)
                ESP_LOGW("usb-host", "Liberación USB: %s", esp_err_to_name(freed));
        }
    }
}
}
