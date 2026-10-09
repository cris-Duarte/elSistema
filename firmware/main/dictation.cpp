#include "dictation.hpp"

#include "model.hpp"

#include "esp_log.h"

namespace el_sistema {
namespace {
constexpr const char* TAG = "dictation";
}

const char* dictation_phase_name(DictationPhase phase)
{
    switch (phase) {
        case DictationPhase::Unconfigured: return "Sin configurar";
        case DictationPhase::Idle: return "Listo";
        case DictationPhase::Preparing: return "Preparando";
        case DictationPhase::Recording: return "Grabando";
        case DictationPhase::Processing: return "Procesando audio";
        case DictationPhase::Connecting: return "Conectando";
        case DictationPhase::Uploading: return "Enviando";
        case DictationPhase::Transcribing: return "Transcribiendo";
        case DictationPhase::LocalResult: return "Muestra local";
        case DictationPhase::Result: return "Resultado";
        case DictationPhase::Error: return "Error";
    }
    return "Error";
}

DictationService::DictationService(AudioService& audio, AudioSessionManager& audio_sessions,
                                   ConnectivityService& connectivity, BleService& ble,
                                   TranscriptionService& transcription)
    : audio_(audio), audio_sessions_(audio_sessions), connectivity_(connectivity), ble_(ble),
      transcription_(transcription)
{
}

bool DictationService::init()
{
    mutex_ = xSemaphoreCreateMutex();
    commands_ = xQueueCreate(12, sizeof(CommandMessage));
    if (mutex_ == nullptr || commands_ == nullptr) return false;
    connectivity_.load();
    snapshot_.phase = connectivity_.configured() ? DictationPhase::Idle : DictationPhase::Unconfigured;
    if (xTaskCreate(task_entry, "dictation", 12288, this, 5, &task_) != pdPASS) return false;
    return true;
}

DictationSnapshot DictationService::snapshot() const
{
    DictationSnapshot result;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        result = snapshot_;
        xSemaphoreGive(mutex_);
    }
    return result;
}

void DictationService::update(const DictationSnapshot& value)
{
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        snapshot_ = value;
        xSemaphoreGive(mutex_);
    }
}

void DictationService::set_phase(DictationPhase phase, const std::string& message)
{
    DictationSnapshot value = snapshot();
    value.phase = phase;
    value.message = message.empty() ? dictation_phase_name(phase) : message;
    if (phase != DictationPhase::Error) value.error.clear();
    update(value);
}

void DictationService::start(AudioSessionOwner owner)
{
    if (owner != AudioSessionOwner::DictationCapture && owner != AudioSessionOwner::AssistantCapture) {
        owner = AudioSessionOwner::DictationCapture;
    }
    send(Command::Start, -1, 0, owner);
}

void DictationService::retry()
{
    send(Command::Retry);
}

void DictationService::start_connectivity() { send(Command::StartConnectivity); }

void DictationService::scan_networks()
{
    send(Command::Scan);
}

void DictationService::reconnect_networks()
{
    send(Command::Reconnect);
}

bool DictationService::connect_and_save(const NetworkCredentials& credentials)
{
    if (busy()) return false;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        pending_credentials_ = credentials;
        xSemaphoreGive(mutex_);
        return send(Command::ConnectAndSave);
    }
    return false;
}

void DictationService::connect_known(size_t index) { send(Command::ConnectKnown, static_cast<int16_t>(index)); }
void DictationService::forget_network(size_t index) { send(Command::ForgetNetwork, static_cast<int16_t>(index)); }
void DictationService::move_network(size_t index, int delta)
{
    send(Command::MoveNetwork, static_cast<int16_t>(index), static_cast<int8_t>(delta));
}
void DictationService::discover_servers() { send(Command::DiscoverServers); }

bool DictationService::select_server(const std::string& id)
{
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        pending_server_id_ = id;
        xSemaphoreGive(mutex_);
        return send(Command::SelectServer);
    }
    return false;
}

void DictationService::test_server()
{
    send(Command::TestServer);
}

void DictationService::stop() { stop_requested_.store(true); }
void DictationService::cancel() { cancel_requested_.store(true); }

void DictationService::discard()
{
    cancel_requested_.store(true);
    if (busy()) return;
    reset_to_idle();
}

void DictationService::reset_to_idle()
{
    audio_buffer_.clear();
    DictationSnapshot value;
    value.phase = connectivity_.configured() ? DictationPhase::Idle : DictationPhase::Unconfigured;
    value.message = dictation_phase_name(value.phase);
    value.has_audio = false;
    update(value);
}

std::string DictationService::accept_result()
{
    DictationSnapshot value = snapshot();
    const std::string result = value.candidate;
    audio_buffer_.clear();
    value = {};
    value.phase = connectivity_.configured() ? DictationPhase::Idle : DictationPhase::Unconfigured;
    value.message = dictation_phase_name(value.phase);
    update(value);
    return result;
}

bool DictationService::play_last_capture()
{
    if (!has_audio() || busy()) return false;
    return send(Command::Play);
}

bool DictationService::busy() const
{
    const DictationPhase phase = snapshot().phase;
    return phase == DictationPhase::Preparing || phase == DictationPhase::Recording ||
           phase == DictationPhase::Processing || phase == DictationPhase::Connecting ||
           phase == DictationPhase::Uploading || phase == DictationPhase::Transcribing;
}

void DictationService::task_entry(void* arg) { static_cast<DictationService*>(arg)->run(); }

bool DictationService::send(Command command, int16_t index, int8_t delta, AudioSessionOwner audio_owner)
{
    const CommandMessage message {command, index, delta, audio_owner};
    return commands_ != nullptr && xQueueSend(commands_, &message, 0) == pdTRUE;
}

void DictationService::run()
{
    while (true) {
        CommandMessage command;
        if (xQueueReceive(commands_, &command, pdMS_TO_TICKS(250)) == pdTRUE) handle(command);
        connectivity_.maintain();
        // Connection/reconnection and explicit commands take precedence. A
        // single coalesced request prevents a scan backlog during dictation.
        if (uxQueueMessagesWaiting(commands_) == 0) {
            const uint32_t session = radar_requested_.exchange(0);
            if (session && session == radar_session_.load()) {
                RadarWifiBatch batch;
                batch.session = session;
                connectivity_.scan_observations(batch.observations, batch.error);
                if (session == radar_session_.load() && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                    batch.generation = radar_batch_.generation + 1;
                    radar_batch_ = std::move(batch);
                    xSemaphoreGive(mutex_);
                }
            }
        }
    }
}

void DictationService::radar_session(uint32_t session)
{
    radar_session_.store(session);
    radar_requested_.store(0);
}
bool DictationService::request_radar_scan(uint32_t session)
{
    if (!session || session != radar_session_.load()) return false;
    uint32_t empty = 0;
    return radar_requested_.compare_exchange_strong(empty, session);
}
RadarWifiBatch DictationService::radar_wifi_batch(uint32_t after_generation) const
{
    RadarWifiBatch result;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        if (radar_batch_.generation != after_generation) result = radar_batch_;
        xSemaphoreGive(mutex_);
    }
    return result;
}

void DictationService::handle(const CommandMessage& message)
{
    switch (message.command) {
        case Command::Start:
            record_and_transcribe(message.audio_owner == AudioSessionOwner::AssistantCapture
                                      ? AudioSessionOwner::AssistantCapture
                                      : AudioSessionOwner::DictationCapture);
            break;
        case Command::Retry: transcribe_current(); break;
        case Command::StartConnectivity:
            connectivity_.start_background();
            // El mismo ESP32-C6 transporta Wi-Fi y HCI por SDIO. NimBLE solo
            // puede iniciarse después de que ESP-Hosted haya completado el
            // enlace, por eso ambos pasos viven en este worker y no en LVGL.
            if (!connectivity_.hosted_ready()) {
                ESP_LOGE(TAG, "Se omite BLE porque el enlace P4-C6 no está disponible");
            } else if (!ble_.init()) {
                ESP_LOGE(TAG, "No se pudo iniciar la prueba BLE");
            }
            break;
        case Command::Scan:
            connectivity_.scan();
            break;
        case Command::Reconnect:
            connectivity_.auto_connect(true);
            break;
        case Command::ConnectAndSave: {
            NetworkCredentials credentials;
            if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                credentials = pending_credentials_;
                xSemaphoreGive(mutex_);
            }
            connectivity_.connect_and_save(credentials);
            break;
        }
        case Command::ConnectKnown:
            if (message.index >= 0) connectivity_.connect_known(static_cast<size_t>(message.index));
            break;
        case Command::ForgetNetwork:
            if (message.index >= 0) connectivity_.forget_network(static_cast<size_t>(message.index));
            break;
        case Command::MoveNetwork:
            if (message.index >= 0) connectivity_.move_network(static_cast<size_t>(message.index), message.delta);
            break;
        case Command::DiscoverServers:
            connectivity_.discover_servers(true);
            break;
        case Command::SelectServer: {
            std::string id;
            if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                id = pending_server_id_;
                xSemaphoreGive(mutex_);
            }
            connectivity_.select_server(id);
            break;
        }
        case Command::TestServer:
            connectivity_.test_server();
            break;
        case Command::Play: {
            AudioSessionLease session(audio_sessions_, AudioSessionOwner::DictationPlayback);
            if (!session.acquired()) {
                DictationSnapshot value = snapshot();
                value.phase = DictationPhase::Error;
                value.error = std::string("El audio está ocupado por ") +
                              audio_session_owner_name(audio_sessions_.owner());
                value.has_audio = !audio_buffer_.empty();
                update(value);
                break;
            }
            cancel_requested_.store(false);
            set_phase(DictationPhase::Processing, "Reproduciendo la muestra local…");
            const bool played = audio_.play(audio_buffer_, &cancel_requested_);
            if (cancel_requested_.load()) {
                reset_to_idle();
                break;
            }
            DictationSnapshot value = snapshot();
            value.phase = played ? DictationPhase::LocalResult : DictationPhase::Error;
            value.has_audio = !audio_buffer_.empty();
            value.error = played ? "" : audio_.last_error();
            value.message = played ? "Reproducción terminada; puedes transcribir o descartar la muestra"
                                   : "No fue posible reproducir la muestra";
            update(value);
            break;
        }
    }
}

void DictationService::record_and_transcribe(AudioSessionOwner owner)
{
    stop_requested_.store(false);
    cancel_requested_.store(false);
    audio_buffer_.clear();
    set_phase(DictationPhase::Preparing);
    if (!audio_.available() && !audio_.init()) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Error;
        value.error = audio_.last_error();
        update(value);
        return;
    }
    AudioSessionLease session(audio_sessions_, owner);
    if (!session.acquired()) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Error;
        value.error = std::string("El audio está ocupado por ") +
                      audio_session_owner_name(audio_sessions_.owner());
        update(value);
        return;
    }
    set_phase(DictationPhase::Recording, "Habla ahora; vuelve a tocar MIC para detener");
    const CaptureEnd ended = audio_.capture(audio_buffer_, stop_requested_, cancel_requested_, [this](const AudioLevel& level) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Recording;
        value.recorded_ms = level.elapsed_ms;
        value.level = level.rms;
        value.peak = level.peak;
        value.slot_levels = level.slot_rms;
        value.slot_peaks = level.slot_peak;
        value.slot_min = level.slot_min;
        value.slot_max = level.slot_max;
        value.slot_nonzero = level.slot_nonzero;
        value.bytes_read = level.bytes_read;
        value.microphone_slot_a = level.microphone_slot_a;
        value.microphone_slot_b = level.microphone_slot_b;
        value.reference_slot = level.reference_slot;
        update(value);
    });
    session.release();
    if (ended != CaptureEnd::Cancelled) set_phase(DictationPhase::Processing, "Finalizando audio…");
    if (ended == CaptureEnd::Cancelled) {
        reset_to_idle();
        return;
    }
    if (ended == CaptureEnd::NoVoice) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Error;
        value.error = "No se detectó voz en cinco segundos";
        update(value);
        return;
    }
    if (ended == CaptureEnd::Error || audio_buffer_.empty()) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Error;
        value.error = audio_.last_error();
        update(value);
        return;
    }
    if (!connectivity_.configured()) {
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::LocalResult;
        value.has_audio = true;
        value.error.clear();
        value.message = "Captura local lista; valida los slots, reprodúcela o configura Whisper";
        update(value);
        return;
    }
    transcribe_current();
}

void DictationService::transcribe_current()
{
    if (audio_buffer_.empty()) {
        set_phase(DictationPhase::Error, "No queda audio para reintentar");
        return;
    }
    cancel_requested_.store(false);
    set_phase(DictationPhase::Processing);
    set_phase(DictationPhase::Connecting, "Localizando la red y el MacBook…");
    if (!connectivity_.ensure_ready()) {
        DictationSnapshot value = snapshot();
        const ConnectivitySnapshot network = connectivity_.snapshot();
        value.phase = DictationPhase::Error;
        value.error = network.error.empty() ? network.message : network.error;
        value.message = "El audio se conserva para reintentar";
        value.has_audio = true;
        update(value);
        return;
    }
    std::string text;
    std::string error;
    const bool ok = transcription_.transcribe(connectivity_.inference_url(), audio_buffer_, cancel_requested_, text,
                                               error, [this](float progress, const char* phase) {
        DictationSnapshot value = snapshot();
        value.phase = std::string(phase) == "Transcribiendo" ? DictationPhase::Transcribing
                                                             : DictationPhase::Uploading;
        value.progress = progress;
        value.message = phase;
        update(value);
    });
    if (!ok) {
        if (cancel_requested_.load()) {
            reset_to_idle();
            return;
        }
        // El audio permanece en PSRAM. Se refresca Bonjour para que el próximo
        // toque en Reintentar use la IP actual del mismo MacBook.
        connectivity_.discover_servers(true);
        DictationSnapshot value = snapshot();
        value.phase = DictationPhase::Error;
        value.error = error;
        value.message = "Puedes reintentar sin volver a grabar";
        value.has_audio = true;
        update(value);
        return;
    }
    DictationSnapshot value = snapshot();
    value.phase = DictationPhase::Result;
    value.has_audio = true;
    value.candidate = text;
    value.message = utf8_codepoint_count(text) > 80
                        ? "El título supera 80 caracteres; acórtalo antes de guardar"
                        : "Revisa el título antes de guardar";
    value.error.clear();
    update(value);
    ESP_LOGI(TAG, "Transcripción recibida (%u bytes UTF-8)", static_cast<unsigned>(text.size()));
}

}  // namespace el_sistema
