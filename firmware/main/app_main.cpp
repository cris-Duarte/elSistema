#include "hardware.hpp"
#include "hourglass.hpp"
#include "radar.hpp"
#include "m5_platform.hpp"
#include "assistant.hpp"
#include "assistant_voice.hpp"
#include "audio.hpp"
#include "audio_session.hpp"
#include "ble.hpp"
#include "connectivity.hpp"
#include "dictation.hpp"
#include "transcription.hpp"
#include "model.hpp"
#include "repository.hpp"
#include "services.hpp"
#include "ui.hpp"

#include <memory>

#include "bsp/m5stack_tab5.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lvgl.h"

#ifdef EL_SISTEMA_RUN_TESTS
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "unity.h"
#endif

namespace {

constexpr const char* TAG = "el_sistema";

#ifdef EL_SISTEMA_RUN_TESTS
// The Home comparison tests hold three 150-particle scenes simultaneously.
// Reserve their stack only in the test build; the production app is unchanged.
constexpr uint32_t kTestTaskStack = 24576;
constexpr int kTestTaskPriority = 5;

SemaphoreHandle_t tests_finished = nullptr;

// Unity escribe su salida detallada por la UART del ROM (UART0), que en el Tab5
// no esta expuesta por USB. Por eso aqui se ejecuta cada prueba del registro y
// se informa el resultado a traves del log de IDF, que si llega al puerto USB.
void domain_tests_task(void*)
{
    const int registered = unity_get_test_count();
    ESP_LOGI(TAG, "Pruebas de dominio registradas: %d", registered);
    if (registered == 0) {
        ESP_LOGE(TAG, "No hay ninguna prueba registrada; revisa WHOLE_ARCHIVE en main/test/CMakeLists.txt");
    }
    for (int index = 0; index < registered; ++index) {
        test_desc_t info = {};
        if (!unity_get_test_info(index, &info)) continue;
        const auto failures_before = Unity.TestFailures;
        unity_run_test_by_index(index);
        ESP_LOGI(TAG, "  %-58s %s", info.name, Unity.TestFailures > failures_before ? "FALLO" : "OK");
        vTaskDelay(1); // Let idle/watchdog tasks run between CPU-only tests.
    }
    ESP_LOGI(TAG, "Resultado de pruebas: %d ejecutadas, %d fallidas", static_cast<int>(Unity.NumberOfTests),
             static_cast<int>(Unity.TestFailures));
    xSemaphoreGive(tests_finished);
    vTaskDelete(nullptr);
}

void run_domain_tests()
{
    tests_finished = xSemaphoreCreateBinary();
    if (tests_finished == nullptr) {
        ESP_LOGE(TAG, "No se pudo crear el semáforo de pruebas");
        return;
    }
    if (xTaskCreate(domain_tests_task, "domain_tests", kTestTaskStack, nullptr, kTestTaskPriority, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea de pruebas");
        vSemaphoreDelete(tests_finished);
        tests_finished = nullptr;
        return;
    }
    xSemaphoreTake(tests_finished, portMAX_DELAY);
    vSemaphoreDelete(tests_finished);
    tests_finished = nullptr;
}
#endif

el_sistema::AppState state;
el_sistema::MissionRepository repository;
el_sistema::ClockService clock_service;
el_sistema::OrientationService orientation_service;
el_sistema::HourglassService hourglass_service(orientation_service);
el_sistema::MazeService maze_service(orientation_service);
el_sistema::ButtonService button_service;
el_sistema::PowerService power_service;
el_sistema::AssistantService assistant_service;
el_sistema::AudioService audio_service;
el_sistema::AudioSessionManager audio_sessions;
el_sistema::TetrisService tetris_service(audio_service, audio_sessions);
el_sistema::ConnectivityService connectivity_service;
el_sistema::BleService ble_service;
el_sistema::TranscriptionService transcription_service;
std::unique_ptr<el_sistema::DictationService> dictation_service;
std::unique_ptr<el_sistema::RadarService> radar_service;
std::unique_ptr<el_sistema::AssistantVoiceService> assistant_voice_service;
std::unique_ptr<el_sistema::ApplicationService> application;
std::unique_ptr<el_sistema::AppUi> ui;

#ifdef EL_SISTEMA_RADAR_SMOKE
void radar_smoke_task(void*)
{
    for (int i = 0; i < 120; ++i) {
        const auto phase = ble_service.snapshot().phase;
        if (phase == el_sistema::BlePhase::Advertising || phase == el_sistema::BlePhase::Connected) break;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    const bool connected_before = connectivity_service.connected();
    if (bsp_display_lock(1000)) { ui->radar_validation_view(true); bsp_display_unlock(); }
    ESP_LOGI(TAG, "Radar smoke: iniciando observación real durante 30 s");
    for (int i = 0; i < 6; ++i) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        const auto snapshot = radar_service->snapshot();
        size_t wifi = 0, ble = 0;
        for (const auto& emitter : snapshot.emitters) {
            if (emitter.observation.kind == el_sistema::RadarKind::Wifi) ++wifi; else ++ble;
        }
        ESP_LOGI(TAG, "Radar smoke: Wi-Fi=%u BLE=%u conectado=%d; %s; %s",
                 static_cast<unsigned>(wifi), static_cast<unsigned>(ble), connectivity_service.connected(),
                 snapshot.wifi_status.c_str(), snapshot.ble_status.c_str());
        if (!snapshot.emitters.empty() && !snapshot.selected_id) radar_service->select(snapshot.emitters.front().id);
        if (i == 2 && bsp_display_lock(1000)) {
            ui->radar_validation_view(true, 0); bsp_display_unlock();
            ESP_LOGI(TAG, "Radar smoke: comprobando composición vertical 720x1280");
        }
    }
    if (bsp_display_lock(1000)) { ui->radar_validation_view(false); bsp_display_unlock(); }
    vTaskDelay(pdMS_TO_TICKS(2000));
    const auto stopped = radar_service->snapshot();
    ESP_LOGI(TAG, "Radar smoke: salida active=%d BLE_scan=%d; conexión previa=%d actual=%d; heap interno=%lu",
             stopped.active, ble_service.observation_active(), connected_before, connectivity_service.connected(),
             static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    vTaskDelete(nullptr);
}
#endif

lv_display_t* start_display()
{
    bsp_display_cfg_t config = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        },
    };
    lv_display_t* display = bsp_display_start_with_config(&config);
    if (display == nullptr) {
        ESP_LOGE(TAG, "No se pudo iniciar el display");
        abort();
    }
    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_ERROR_CHECK(bsp_display_brightness_set(72));
    return display;
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando El Sistema v0.4.2 · Asistente A2");

#ifdef EL_SISTEMA_RUN_TESTS
    ESP_LOGI(TAG, "Modo de pruebas activado: ejecutando pruebas de dominio");
    run_domain_tests();
    ESP_LOGI(TAG, "Pruebas de dominio finalizadas");
#endif

    if (!repository.init()) {
        ESP_LOGE(TAG, "No se pudo preparar el almacenamiento: %s", repository.last_error());
    }
    const bool restored = repository.load(state);

    const bool m5_ready = el_sistema::init_m5_platform();
    if (!m5_ready) {
        ESP_LOGW(TAG, "M5Unified no disponible; usando BSP sin movimiento");
        ESP_ERROR_CHECK(bsp_i2c_init());
    }
    bsp_io_expander_pi4ioe_init(bsp_i2c_get_handle());
    // BSP initialization clears CHG_EN after M5Unified enabled it. Restore
    // charging before workers start; retain the charger's existing limits.
    bsp_set_charge_en(true);
    bsp_reset_tp();

    clock_service.init(bsp_i2c_get_handle());
    if (m5_ready) orientation_service.init(bsp_i2c_get_handle());
    button_service.init(bsp_i2c_get_handle());
    power_service.init(bsp_i2c_get_handle());
    application = std::make_unique<el_sistema::ApplicationService>(state, repository, clock_service);
    dictation_service = std::make_unique<el_sistema::DictationService>(audio_service, audio_sessions,
                                                                       connectivity_service, ble_service,
                                                                       transcription_service);
    if (!dictation_service->init()) ESP_LOGE(TAG, "No se pudo iniciar el coordinador de dictado");
    assistant_voice_service = std::make_unique<el_sistema::AssistantVoiceService>(audio_service, audio_sessions,
                                                                                  *dictation_service);
    if (!assistant_voice_service->init()) ESP_LOGE(TAG, "No se pudo iniciar el coordinador de voz del asistente");
    if (!restored) application->seed_first_run();

    lv_display_t* display = start_display();
    hourglass_service.init();
    if (!tetris_service.init()) ESP_LOGE(TAG, "No se pudo iniciar Tetris");
    if (!maze_service.init()) ESP_LOGE(TAG, "No se pudo iniciar Laberinto");
    radar_service = std::make_unique<el_sistema::RadarService>(*dictation_service, ble_service, audio_service, audio_sessions);
    if (!radar_service->init()) ESP_LOGE(TAG, "No se pudo iniciar Radar");
    ui = std::make_unique<el_sistema::AppUi>(state, *application, repository, clock_service, orientation_service,
                                             button_service, *dictation_service, power_service, assistant_service,
                                            *assistant_voice_service, hourglass_service, *radar_service, tetris_service, maze_service);
    bsp_display_lock(0);
    ui->start(display);
    bsp_display_unlock();

    // El enlace P4-C6 y los escaneos pueden tardar varios segundos. Arrancan
    // únicamente cuando LVGL ya mostró la interfaz y siempre en el worker.
    dictation_service->start_connectivity();
#ifdef EL_SISTEMA_RADAR_SMOKE
    xTaskCreate(radar_smoke_task, "radar_smoke", 12288, nullptr, 2, nullptr);
#endif

    ESP_LOGI(TAG, "Interfaz y servicios listos");
}
