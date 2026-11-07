#pragma once
#include "camera_model.hpp"
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "usb/uvc_host.h"

namespace el_sistema {
struct CameraUsbFrame { const uvc_host_frame_t* frame = nullptr; int64_t arrived_us = 0; };
struct CameraUsbDevice { uint8_t address = 0, stream_index = 0; };

// All lifecycle methods belong to CameraService's worker, never LVGL/callbacks.
class CameraUsbTransport {
public:
    bool begin(char* error, size_t capacity);
    bool next_device(CameraUsbDevice& device);
    bool open(const CameraUsbDevice& device, CameraSnapshot& status);
    bool take(CameraUsbFrame& frame);
    void give_back(CameraUsbFrame& frame);
    bool close();
    bool end();
    bool disconnected() const { return disconnected_.load(); }
    uint32_t received() const { return received_.load(); }
    uint32_t dropped() const { return dropped_.load(); }
    uint32_t errors() const { return errors_.load(); }
private:
    static void driver_event(const uvc_host_driver_event_data_t*, void*);
    static void stream_event(const uvc_host_stream_event_data_t*, void*);
    static bool frame_event(const uvc_host_frame_t*, void*);
    QueueHandle_t devices_ = nullptr, frames_ = nullptr;
    uvc_host_stream_hdl_t stream_ = nullptr;
    bool installed_ = false;
    std::atomic<bool> accepting_{false}, disconnected_{false};
    std::atomic<uint32_t> received_{0}, dropped_{0}, errors_{0};
};
}
