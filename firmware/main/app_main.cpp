#include "hardware.hpp"
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
constexpr uint32_t kTestTaskStack = 8192;
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
el_sistema::ButtonService button_service;
el_sistema::PowerService power_service;
el_sistema::AssistantService assistant_service;
el_sistema::AudioService audio_service;
el_sistema::AudioSessionManager audio_sessions;
el_sistema::ConnectivityService connectivity_service;
el_sistema::BleService ble_service;
el_sistema::TranscriptionService transcription_service;
std::unique_ptr<el_sistema::DictationService> dictation_service;
std::unique_ptr<el_sistema::AssistantVoiceService> assistant_voice_service;
std::unique_ptr<el_sistema::ApplicationService> application;
std::unique_ptr<el_sistema::AppUi> ui;

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
    ui = std::make_unique<el_sistema::AppUi>(state, *application, repository, clock_service, orientation_service,
                                             button_service, *dictation_service, power_service, assistant_service,
                                             *assistant_voice_service);
    bsp_display_lock(0);
    ui->start(display);
    bsp_display_unlock();

    // El enlace P4-C6 y los escaneos pueden tardar varios segundos. Arrancan
    // únicamente cuando LVGL ya mostró la interfaz y siempre en el worker.
    dictation_service->start_connectivity();

    ESP_LOGI(TAG, "Interfaz y servicios listos");
}
