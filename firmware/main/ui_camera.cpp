#include "ui.hpp"
#include "fonts/fonts.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <cstdio>
#include <string>

namespace el_sistema {
namespace {
void clean_box(lv_obj_t* object, int x, int y, int width, int height)
{
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}
void text_if_changed(lv_obj_t* object, const char* text)
{
    if (object && std::string(lv_label_get_text(object)) != text) lv_label_set_text(object, text);
}
}
void AppUi::release_camera_frame()
{
    if (camera_frame_.slot < 0) return;
    if (camera_image_) lv_image_set_src(camera_image_, nullptr);
    lv_image_cache_drop(&camera_images_[camera_frame_.slot]);
    camera_.release_frame(camera_frame_.slot);
    camera_frame_ = {};
}
void AppUi::render_camera()
{
    auto* surface = lv_obj_create(screen_); clean_box(surface, 0, 0, 1280, 720);
    lv_obj_set_style_bg_color(surface, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(surface, LV_OPA_COVER, 0);
    lv_obj_add_flag(surface, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(surface, camera_touch_callback, LV_EVENT_CLICKED, this);
    camera_image_ = lv_image_create(surface);
    lv_obj_remove_flag(camera_image_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(camera_image_, 0, 0);
    camera_cover_ = lv_obj_create(surface); clean_box(camera_cover_, 0, 0, 1280, 720);
    lv_obj_set_style_bg_color(camera_cover_, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(camera_cover_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(camera_cover_, LV_OBJ_FLAG_CLICKABLE);
    camera_state_label_ = label(camera_cover_, "PREPARANDO", &font_rajdhani_64, 0xE8EDF0);
    lv_obj_set_pos(camera_state_label_, 72, 150);
    camera_error_label_ = label(camera_cover_, "", &font_rajdhani_28, 0xA5C2D1);
    lv_obj_set_pos(camera_error_label_, 72, 250); lv_obj_set_width(camera_error_label_, 1136);

    camera_controls_ = lv_obj_create(surface); clean_box(camera_controls_, 0, 0, 1280, 122);
    lv_obj_set_style_bg_color(camera_controls_, lv_color_hex(0x10151C), 0);
    lv_obj_set_style_bg_opa(camera_controls_, LV_OPA_90, 0);
    button(camera_controls_, "INICIO", 16, 12, 160, 54, ActionCode::GoHome);
    button(camera_controls_, "CONEXIÓN", 188, 12, 220, 54, ActionCode::CameraConnection);
    button(camera_controls_, "REINTENTAR", 420, 12, 220, 54, ActionCode::CameraRetry);
    button(camera_controls_, "OCULTAR", 1060, 12, 204, 54, ActionCode::CameraHideControls);
    camera_metrics_ = label(camera_controls_, "PRUEBA TÉCNICA · SIN VALIDAR CON LA SONY", &font_rajdhani_20, 0xBCCBD7);
    lv_obj_set_pos(camera_metrics_, 20, 80); lv_obj_set_width(camera_metrics_, 1240);

    if (camera_details_) {
        camera_connection_ = panel(surface, 80, 140, 1120, 540, true);
        auto* heading = label(camera_connection_, "SONY ZV-E10 II · PRUEBA USB", &font_rajdhani_40, 0xE8EDF0);
        lv_obj_set_pos(heading, 20, 16);
        auto* help = label(camera_connection_,
            "USB-A de Tab5 → USB-C de Sony (cable de datos).\n"
            "En Sony: USB Streaming · HD 720p 25/30 · Movie Rec During Streaming: Enable.\n"
            "USB Power Supply: Off. Ambas con batería/alimentación propia.\n"
            "Comprobar 4K interno 25/30 y continuidad al pulsar REC en la Sony.\n"
            "Picture Profile no disponible. Tab5 no controla ni indica REC.", &font_rajdhani_28, 0xBCCBD7);
        lv_obj_set_pos(help, 20, 76); lv_obj_set_width(help, 1060);
        auto* formats = label(camera_connection_, "FORMATOS ANUNCIADOS POR USB", &font_rajdhani_28, 0xA5C2D1);
        lv_obj_set_pos(formats, 20, 310);
        camera_formats_ = label(camera_connection_, "Conecta la Sony para enumerar formatos.", &font_rajdhani_20, 0xBCCBD7);
        lv_obj_set_pos(camera_formats_, 20, 350); lv_obj_set_width(camera_formats_, 1060);
    }
    camera_metrics_at_ = 0;
    camera_presented_ = 0; camera_present_since_ = esp_timer_get_time(); camera_present_fps_ = 0;
    camera_timer_ = lv_timer_create(camera_timer_callback, 16, this);
    update_camera();
}
void AppUi::camera_timer_callback(lv_timer_t* timer)
{
    static_cast<AppUi*>(lv_timer_get_user_data(timer))->update_camera();
}
void AppUi::camera_touch_callback(lv_event_t* event)
{
    auto* ui = static_cast<AppUi*>(lv_event_get_user_data(event));
    if (ui->overlay_ != Overlay::None) return;
    ui->camera_controls_until_ = esp_timer_get_time() + 5000000;
    ui->update_camera();
}
void AppUi::update_camera()
{
    if (view_ != View::Camera || screen_off_ || render_pending_ || !camera_image_) return;
    const auto current = camera_.snapshot();
    if (current.available) camera_snapshot_ = current;
    const auto& state = camera_snapshot_;
    const int64_t now = esp_timer_get_time();
    const bool live = state.available && state.phase == CameraPhase::Live && camera_frame_fresh(state.last_frame_us, now);
    if (live) {
        const CameraFrame next = camera_.acquire_frame();
        if (next.slot >= 0) {
            auto& image = camera_images_[next.slot];
            image = {}; image.header.magic = LV_IMAGE_HEADER_MAGIC; image.header.cf = LV_COLOR_FORMAT_RGB565;
            image.header.w = CAMERA_WIDTH; image.header.h = CAMERA_HEIGHT; image.header.stride = CAMERA_WIDTH * 2;
            image.data_size = CAMERA_RGB_BYTES; image.data = next.pixels;
            lv_image_cache_drop(&image);
            lv_image_set_src(camera_image_, &image);
            lv_obj_invalidate(camera_image_);
            // Both image descriptors are stable AppUi members. Only after LVGL
            // stops referencing the previous one may the worker overwrite it.
            const int previous = camera_frame_.slot;
            if (previous >= 0) {
                lv_image_cache_drop(&camera_images_[previous]); camera_.release_frame(previous);
            }
            camera_frame_ = next; ++camera_presented_;
        }
        lv_obj_add_flag(camera_cover_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(camera_cover_, LV_OBJ_FLAG_HIDDEN);
        release_camera_frame();
        text_if_changed(camera_state_label_, state.available ? camera_phase_text(state.phase) : "Monitor no disponible");
        text_if_changed(camera_error_label_, state.error[0] ? state.error :
            "Conecta la Sony al USB-A y activa USB Streaming a 720p.\n"
            "Toca CONEXIÓN para ver los pasos de preparación.\n"
            "Esta versión verifica la señal; las ayudas de encuadre vendrán después.");
    }
    const bool controls = !live || camera_details_ || overlay_ != Overlay::None || now < camera_controls_until_;
    if (controls) lv_obj_remove_flag(camera_controls_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(camera_controls_, LV_OBJ_FLAG_HIDDEN);
    if (now - camera_present_since_ >= 1000000) {
        camera_present_fps_ = camera_presented_ * 1000000.f / (now-camera_present_since_);
        camera_presented_ = 0; camera_present_since_ = now;
    }
    if (now - camera_metrics_at_ >= 250000) {
        camera_metrics_at_ = now;
        char metrics[256];
        std::snprintf(metrics, sizeof(metrics),
            "PRUEBA USB · 720p MJPEG %.2f fps · decode %.1f · vista %.1f · descartados %lu · JPEG %.1f ms · %.2f MiB",
            state.source_fps, state.decoded_fps, camera_present_fps_, (unsigned long)state.dropped,
            state.decode_us / 1000.f, state.video_bytes / 1048576.f);
        text_if_changed(camera_metrics_, metrics);
        if (camera_formats_) text_if_changed(camera_formats_, state.formats[0] ? state.formats : "Esperando descriptores de la Sony.");
    }
}
void AppUi::leave_camera()
{
    camera_.set_visible(false);
    const int detected = orientation_.poll();
    current_rotation_ = detected >= 0 ? detected : camera_return_rotation_;
    orientation_.set_current(current_rotation_);
    static const lv_display_rotation_t rotations[] = {LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90,
                                                     LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270};
    lv_display_set_rotation(display_, rotations[current_rotation_]);
    home_rotation_pending_ = false; home_rotation_sources_.clear();
}
}
