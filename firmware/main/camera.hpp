#pragma once
#include "camera_model.hpp"
#include "camera_usb.hpp"
#include <atomic>
#include "driver/jpeg_decode.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace el_sistema {
class CameraService {
public:
    bool init();
    void set_visible(bool visible);
    void retry();
    CameraSnapshot snapshot();
    CameraFrame acquire_frame();
    void release_frame(int slot);
private:
    static void task_entry(void*);
    void run();
    void publish();
    bool allocate_video();
    bool free_video();
    bool decode(const CameraUsbFrame& frame);
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t worker_ = nullptr;
    std::atomic<bool> visible_{false}, retry_{false};
    CameraSnapshot published_{}, state_{};
    CameraFrameSlots slots_;
    std::array<uint8_t*,2> pixels_{};
    size_t rgb_size_ = 0;
    jpeg_decoder_handle_t decoder_ = nullptr;
    CameraUsbTransport usb_;
    uint32_t session_ = 0, sequence_ = 0;
};
}
