#include "camera.hpp"
#include "camera_pixel_format.hpp"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace el_sistema {
bool CameraService::init()
{
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        published_.available = true; published_.phase = CameraPhase::Error;
        std::snprintf(published_.error, sizeof(published_.error), "No hay memoria para iniciar Cámara. Reinicia la Tab5.");
        return false;
    }
    if (xTaskCreate(task_entry, "camera", 12288, this, 3, &worker_) != pdPASS) {
        vSemaphoreDelete(mutex_); mutex_ = nullptr;
        published_.available = true; published_.phase = CameraPhase::Error;
        std::snprintf(published_.error, sizeof(published_.error), "No se pudo crear el worker de Cámara. Reinicia la Tab5.");
        return false;
    }
    return true;
}
void CameraService::set_visible(bool visible) { visible_.store(visible); }
void CameraService::retry() { retry_.store(true); }
CameraSnapshot CameraService::snapshot()
{
    if (!mutex_) return published_; // Immutable startup error; never a dangling mutex.
    CameraSnapshot result;
    if (mutex_ && xSemaphoreTake(mutex_, 0) == pdTRUE) {
        result = published_; xSemaphoreGive(mutex_);
        if (result.phase == CameraPhase::Live && !camera_frame_fresh(result.last_frame_us, esp_timer_get_time()))
            result.phase = CameraPhase::NoSignal;
    }
    return result;
}
CameraFrame CameraService::acquire_frame()
{
    CameraFrame frame;
    if (!mutex_ || !visible_ || xSemaphoreTake(mutex_, 0) != pdTRUE) return frame;
    if (published_.phase == CameraPhase::Live && camera_frame_fresh(published_.last_frame_us, esp_timer_get_time())) {
        frame.slot = slots_.acquire_latest(published_.session);
        if (frame.slot >= 0) {
            frame.pixels = pixels_[frame.slot]; frame.sequence = slots_.sequence(frame.slot);
            frame.timestamp_us = slots_.timestamp(frame.slot);
        }
    }
    xSemaphoreGive(mutex_);
    return frame;
}
void CameraService::release_frame(int slot)
{
    if (!mutex_ || slot < 0) return;
    // Short critical section only; no decoding, USB or flash operation holds it.
    xSemaphoreTake(mutex_, portMAX_DELAY); slots_.release(slot); xSemaphoreGive(mutex_);
}
void CameraService::publish()
{
    state_.available = true;
    xSemaphoreTake(mutex_, portMAX_DELAY); published_ = state_; xSemaphoreGive(mutex_);
}
bool CameraService::allocate_video()
{
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 65536 ||
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 24576 ||
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < CAMERA_MEMORY_LIMIT) {
        std::snprintf(state_.error, sizeof(state_.error), "Memoria insuficiente para un monitor seguro. Los otros módulos siguen disponibles.");
        return false;
    }
    jpeg_decode_engine_cfg_t config{};
    config.timeout_ms = 100;
    const auto err = jpeg_new_decoder_engine(&config, &decoder_);
    if (err != ESP_OK) {
        std::snprintf(state_.error, sizeof(state_.error), "Decodificador JPEG: %s", esp_err_to_name(err)); return false;
    }
    const jpeg_decode_memory_alloc_cfg_t allocation{JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    for (auto& buffer : pixels_) {
        buffer = static_cast<uint8_t*>(jpeg_alloc_decoder_mem(CAMERA_RGB_BYTES, &allocation, &rgb_size_));
        if (!buffer || !esp_ptr_external_ram(buffer)) {
            std::snprintf(state_.error, sizeof(state_.error), "No se pudieron reservar los buffers RGB565 en PSRAM.");
            return false;
        }
    }
    state_.video_bytes = rgb_size_ * 2;
    if (state_.video_bytes + 3 * CAMERA_JPEG_LIMIT + 65536 > CAMERA_MEMORY_LIMIT) {
        std::snprintf(state_.error, sizeof(state_.error), "El video supera el presupuesto de 8 MiB."); return false;
    }
    return true;
}
bool CameraService::free_video()
{
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (!slots_.idle()) { xSemaphoreGive(mutex_); return false; }
    slots_.reset();
    const auto buffers = pixels_; pixels_.fill(nullptr);
    xSemaphoreGive(mutex_);
    for (auto* buffer : buffers) heap_caps_free(buffer);
    if (decoder_) { jpeg_del_decoder_engine(decoder_); decoder_ = nullptr; }
    rgb_size_ = 0; state_.video_bytes = 0;
    return true;
}
bool CameraService::decode(const CameraUsbFrame& frame)
{
    if (!frame.frame || frame.frame->vs_format.h_res != CAMERA_WIDTH || frame.frame->vs_format.v_res != CAMERA_HEIGHT ||
        frame.frame->vs_format.format != UVC_VS_FORMAT_MJPEG || !camera_valid_jpeg(frame.frame->data, frame.frame->data_len)) {
        ++state_.invalid;
        return false;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const int slot = slots_.begin_write(); xSemaphoreGive(mutex_);
    if (slot < 0) { ++state_.dropped; return true; }
    const int64_t start = esp_timer_get_time();
    jpeg_decode_cfg_t config{};
    config.output_format = CAMERA_JPEG_PIXEL_FORMAT;
    config.rgb_order = CAMERA_JPEG_PIXEL_ORDER;
    // JPEG/JFIF YCbCr matrix. Validate colors on the physical camera before
    // exposing exposure/focus tools; no automatic claims about color accuracy.
    config.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
    uint32_t bytes = 0;
    const auto err = jpeg_decoder_process(decoder_, &config, frame.frame->data, frame.frame->data_len,
                                          pixels_[slot], rgb_size_, &bytes);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (err == ESP_OK && bytes == CAMERA_RGB_BYTES && visible_ && !usb_.disconnected()) {
        slots_.publish(slot, session_, ++sequence_, frame.arrived_us);
        ++state_.decoded; state_.last_frame_us = frame.arrived_us;
        state_.decode_us = static_cast<uint32_t>(esp_timer_get_time() - start);
        state_.phase = CameraPhase::Live;
    } else {
        slots_.abort_write(slot); ++state_.invalid;
    }
    xSemaphoreGive(mutex_);
    return err == ESP_OK && bytes == CAMERA_RGB_BYTES;
}
void CameraService::task_entry(void* context) { static_cast<CameraService*>(context)->run(); }
void CameraService::run()
{
    state_.available = true; publish();
    while (true) {
        if (!visible_) { vTaskDelay(pdMS_TO_TICKS(40)); continue; }
        retry_ = false;
        state_ = {}; state_.session = ++session_; state_.phase = CameraPhase::Preparing; publish();
        bool failed = !allocate_video();
        bool started = !failed && usb_.begin(state_.error, sizeof(state_.error));
        failed = failed || !started;
        state_.phase = failed ? CameraPhase::Error : CameraPhase::Waiting; publish();
        bool streaming = false;
        uint32_t consecutive_invalid = 0, last_decoded = 0, local_dropped = 0;
        int64_t fps_since = esp_timer_get_time(), report_since = fps_since;
        while (!failed && visible_ && !retry_) {
            CameraUsbDevice device;
            if (!streaming && usb_.next_device(device)) {
                state_.phase = CameraPhase::Preparing; publish();
                streaming = usb_.open(device, state_);
                if (!streaming) { failed = true; state_.phase = CameraPhase::Error; publish(); break; }
                state_.phase = CameraPhase::NoSignal; publish();
            }
            if (usb_.disconnected()) {
                // Stop and re-enumerate, but never manufacture a "live" frame.
                std::snprintf(state_.error, sizeof(state_.error), "Cámara desconectada. Reconecta el cable o toca REINTENTAR.");
                state_.phase = CameraPhase::Waiting; publish();
                break;
            }
            CameraUsbFrame frame;
            if (streaming && usb_.take(frame)) {
                const uint32_t old_dropped = state_.dropped;
                const bool valid = decode(frame);
                local_dropped += state_.dropped - old_dropped;
                usb_.give_back(frame);
                consecutive_invalid = valid ? 0 : consecutive_invalid + 1;
                if (consecutive_invalid >= 30) {
                    std::snprintf(state_.error, sizeof(state_.error), "MJPEG no compatible o incompleto. Se requiere JPEG baseline 720p válido para el P4.");
                    failed = true; state_.phase = CameraPhase::Error;
                }
            }
            const int64_t now = esp_timer_get_time();
            state_.received = usb_.received();
            xSemaphoreTake(mutex_, portMAX_DELAY);
            state_.dropped = usb_.dropped() + local_dropped + slots_.discarded();
            xSemaphoreGive(mutex_);
            if (now - fps_since >= 1000000) {
                state_.decoded_fps = (state_.decoded-last_decoded) * 1000000.f / (now-fps_since);
                fps_since = now; last_decoded = state_.decoded;
            }
            if (now - report_since >= 5000000) {
                ESP_LOGI("camera", "MJPEG %.2f fps; decodifica %.1f fps; rx=%lu, decode=%lu, descarta=%lu, inválidos=%lu, USBerr=%lu, JPEG=%lu us, reserva=%u, interno=%u, PSRAM=%u",
                         state_.source_fps, state_.decoded_fps, (unsigned long)state_.received, (unsigned long)state_.decoded,
                         (unsigned long)state_.dropped, (unsigned long)state_.invalid, (unsigned long)usb_.errors(),
                         (unsigned long)state_.decode_us, unsigned(state_.video_bytes),
                         unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
                report_since = now;
            }
            publish();
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        if (!visible_ || retry_) state_.phase = CameraPhase::Off;
        publish(); // Allows UI to detach its image BEFORE any buffers are freed.
        if (!usb_.end()) {
            // A failed third-party driver teardown may have partially destroyed
            // its global state. Never reinstall blindly or free retained frames.
            state_.phase = CameraPhase::Error;
            std::snprintf(state_.error, sizeof(state_.error), "No se pudo cerrar USB con seguridad. Vuelve a Inicio y reinicia la Tab5.");
            publish();
            while (!free_video()) vTaskDelay(pdMS_TO_TICKS(10));
            while (true) vTaskDelay(pdMS_TO_TICKS(1000));
        }
        while (!free_video()) { vTaskDelay(pdMS_TO_TICKS(10)); }
        publish();
        // A deliberate failure waits for an explicit retry, not a USB restart storm.
        if (failed) while (visible_ && !retry_) vTaskDelay(pdMS_TO_TICKS(40));
        // Physical disconnects may reconnect automatically while still visible.
        if (started && !failed) vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}
