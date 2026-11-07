#include "camera_usb.hpp"
#include "usb_host_runtime.hpp"
#include "bsp/m5stack_tab5.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <algorithm>
#include <cstdio>

namespace el_sistema {
namespace {
bool failure(char* text, size_t size, const char* operation, esp_err_t err)
{
    std::snprintf(text, size, "%s: %s", operation, esp_err_to_name(err));
    ESP_LOGW("camera-usb", "%s", text); return false;
}
}
bool CameraUsbTransport::begin(char* error, size_t capacity)
{
    if (installed_) return failure(error, capacity, "USB aún ocupado", ESP_ERR_INVALID_STATE);
    if (!devices_) devices_ = xQueueCreate(8, sizeof(CameraUsbDevice));
    if (!frames_) frames_ = xQueueCreate(1, sizeof(CameraUsbFrame));
    if (!devices_ || !frames_) return failure(error, capacity, "Colas USB", ESP_ERR_NO_MEM);
    xQueueReset(devices_); xQueueReset(frames_);
    disconnected_ = false; received_ = 0; dropped_ = 0; errors_ = 0;
    auto err = UsbHostRuntime::start();
    if (err != ESP_OK) return failure(error, capacity, "USB Host", err);
    uvc_host_driver_config_t config{};
    config.driver_task_stack_size = 6144;
    config.driver_task_priority = 5;
    config.xCoreID = tskNO_AFFINITY;
    config.create_background_task = true;
    config.event_cb = driver_event;
    config.user_ctx = this;
    err = uvc_host_install(&config);
    if (err != ESP_OK) return failure(error, capacity, "Controlador UVC", err);
    installed_ = true;
    bsp_set_usb_5v_en(true);
    return true;
}
void CameraUsbTransport::driver_event(const uvc_host_driver_event_data_t* event, void* context)
{
    auto& self = *static_cast<CameraUsbTransport*>(context);
    if (event->type != UVC_HOST_DRIVER_EVENT_DEVICE_CONNECTED) return;
    const CameraUsbDevice device{event->device_connected.dev_addr, event->device_connected.uvc_stream_index};
    if (xQueueSend(self.devices_, &device, 0) != pdTRUE) ++self.errors_;
}
void CameraUsbTransport::stream_event(const uvc_host_stream_event_data_t* event, void* context)
{
    auto& self = *static_cast<CameraUsbTransport*>(context);
    switch (event->type) {
        case UVC_HOST_DEVICE_DISCONNECTED: self.accepting_ = false; self.disconnected_ = true; break;
        case UVC_HOST_TRANSFER_ERROR: ++self.errors_; break;
        case UVC_HOST_FRAME_BUFFER_OVERFLOW: case UVC_HOST_FRAME_BUFFER_UNDERFLOW: ++self.dropped_; break;
        default: break;
    }
}
bool CameraUsbTransport::frame_event(const uvc_host_frame_t* frame, void* context)
{
    auto& self = *static_cast<CameraUsbTransport*>(context);
    if (!self.accepting_) return true;
    ++self.received_;
    const CameraUsbFrame latest{frame, esp_timer_get_time()};
    CameraUsbFrame previous;
    // A single pending frame, plus one being decoded: discard lag rather than
    // queueing seconds of video. No copies, allocations, logs or LVGL here.
    if (xQueueReceive(self.frames_, &previous, 0) == pdTRUE) {
        self.give_back(previous); ++self.dropped_;
    }
    if (xQueueSend(self.frames_, &latest, 0) == pdTRUE) return false;
    ++self.dropped_; return true;
}
bool CameraUsbTransport::next_device(CameraUsbDevice& device)
{
    return devices_ && xQueueReceive(devices_, &device, 0) == pdTRUE;
}
bool CameraUsbTransport::open(const CameraUsbDevice& device, CameraSnapshot& status)
{
    size_t count = 0;
    auto err = uvc_host_get_frame_list(device.address, device.stream_index, nullptr, &count);
    if (err != ESP_OK || !count || count > 128)
        return failure(status.error, sizeof(status.error), "Descriptores UVC", err == ESP_OK ? ESP_ERR_INVALID_SIZE : err);
    auto* formats = static_cast<uvc_host_frame_info_t*>(heap_caps_calloc(count, sizeof(uvc_host_frame_info_t), MALLOC_CAP_SPIRAM));
    if (!formats) return failure(status.error, sizeof(status.error), "Memoria de formatos", ESP_ERR_NO_MEM);
    err = uvc_host_get_frame_list(device.address, device.stream_index,
                                reinterpret_cast<uvc_host_frame_info_t (*)[]>(formats), &count);
    float chosen = 0;
    size_t used = 0;
    if (err == ESP_OK) for (size_t n = 0; n < count; ++n) {
        const auto& f = formats[n];
        const char* encoding = f.format == UVC_VS_FORMAT_MJPEG ? "MJPEG" : "Otro formato";
        const float default_fps = f.default_interval ? 10000000.f / f.default_interval : 0;
        ESP_LOGI("camera-usb", "Interfaz %u: formato %u, %ux%u, predeterminado %.2f fps",
                 device.stream_index, unsigned(f.format), f.h_res, f.v_res, default_fps);
        if (used < sizeof(status.formats)-1) {
            const int written = std::snprintf(status.formats+used, sizeof(status.formats)-used,
                                              "%s %ux%u · %.2f fps\n", encoding, f.h_res, f.v_res, default_fps);
            if (written > 0) used += std::min<size_t>(written, sizeof(status.formats)-1-used);
        }
        if (f.format != UVC_VS_FORMAT_MJPEG || f.h_res != CAMERA_WIDTH || f.v_res != CAMERA_HEIGHT) continue;
        CameraIntervals intervals;
        intervals.count = std::min<size_t>(f.interval_type, intervals.discrete.size());
        if (f.interval_type) {
            intervals.count = std::min<size_t>(intervals.count, CONFIG_UVC_INTERVAL_ARRAY_SIZE);
            for (size_t i = 0; i < intervals.count; ++i) intervals.discrete[i] = f.interval[i];
        } else {
            intervals.minimum = f.interval_min; intervals.maximum = f.interval_max; intervals.step = f.interval_step;
        }
        chosen = std::max(chosen, camera_choose_fps(intervals));
    }
    heap_caps_free(formats);
    if (err != ESP_OK) return failure(status.error, sizeof(status.error), "Lectura de formatos", err);
    if (!chosen) {
        std::snprintf(status.error, sizeof(status.error), "No ofrece MJPEG 1280×720 a 25/30 fps. Revisa USB Streaming en la Sony.");
        return false;
    }
    // The URB payloads/descriptors still need internal DMA-capable RAM even
    // though MJPEG and RGB live in PSRAM. Leave headroom for radio and touch.
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 98304 ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) < 16384)
        return failure(status.error, sizeof(status.error), "Reserva interna insuficiente para USB", ESP_ERR_NO_MEM);
    uvc_host_stream_config_t config{};
    config.event_cb = stream_event; config.frame_cb = frame_event; config.user_ctx = this;
    config.usb.dev_addr = device.address;
    config.usb.vid = 0x054c; // Sony, not an arbitrary peripheral or its mass-storage interface.
    config.usb.uvc_stream_index = device.stream_index;
    config.vs_format = {CAMERA_WIDTH, CAMERA_HEIGHT, chosen, UVC_VS_FORMAT_MJPEG};
    config.advanced.number_of_frame_buffers = 3;
    config.advanced.frame_size = CAMERA_JPEG_LIMIT;
    config.advanced.frame_heap_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    config.advanced.number_of_urbs = 4;
    config.advanced.urb_size = 8192;
    err = uvc_host_stream_open(&config, pdMS_TO_TICKS(1000), &stream_);
    if (err != ESP_OK) return failure(status.error, sizeof(status.error), "Abrir Sony USB", err);
    disconnected_ = false;
    accepting_ = true;
    err = uvc_host_stream_start(stream_);
    if (err != ESP_OK) { accepting_ = false; return failure(status.error, sizeof(status.error), "Iniciar video", err); }
    uvc_host_stream_format_t actual{};
    err = uvc_host_stream_format_get(stream_, &actual);
    if (err != ESP_OK || actual.h_res != CAMERA_WIDTH || actual.v_res != CAMERA_HEIGHT ||
        actual.format != UVC_VS_FORMAT_MJPEG || actual.fps < 24.8f || actual.fps > 30.2f)
        return failure(status.error, sizeof(status.error), "Formato negociado inesperado", ESP_ERR_NOT_SUPPORTED);
    status.source_fps = actual.fps;
    status.video_bytes += 3 * CAMERA_JPEG_LIMIT + 4 * 8192;
    ESP_LOGI("camera-usb", "Negociado MJPEG %ux%u %.2f fps", actual.h_res, actual.v_res, actual.fps);
    return true;
}
bool CameraUsbTransport::take(CameraUsbFrame& frame) { return frames_ && xQueueReceive(frames_, &frame, 0) == pdTRUE; }
void CameraUsbTransport::give_back(CameraUsbFrame& frame)
{
    if (stream_ && frame.frame) {
        if (uvc_host_frame_return(stream_, const_cast<uvc_host_frame_t*>(frame.frame)) != ESP_OK) ++errors_;
    }
    frame = {};
}
bool CameraUsbTransport::close()
{
    accepting_ = false;
    if (!stream_) return true;
    // The worker has already returned its own frame; stop callbacks before
    // draining the last pending one. Never close from a UVC callback.
    uvc_host_stream_stop(stream_);
    CameraUsbFrame frame;
    while (take(frame)) give_back(frame);
    const auto err = uvc_host_stream_close(stream_);
    if (err != ESP_OK) { ESP_LOGW("camera-usb", "Cerrar: %s", esp_err_to_name(err)); return false; }
    stream_ = nullptr;
    return true;
}
bool CameraUsbTransport::end()
{
    if (!close()) return false;
    if (installed_) {
        const auto err = uvc_host_uninstall();
        if (err != ESP_OK) { ESP_LOGW("camera-usb", "Liberar UVC: %s", esp_err_to_name(err)); return false; }
        installed_ = false;
        bsp_set_usb_5v_en(false);
    }
    return true;
}
}
