#include "transcription.hpp"

#include <algorithm>
#include <cstring>

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace el_sistema {
namespace {

constexpr const char* TAG = "transcription";
constexpr const char* kBoundary = "----ElSistemaTab5Boundary7MA4YWxk";
constexpr size_t kMaxResponse = 4096;

bool write_all(esp_http_client_handle_t client, const uint8_t* data, size_t size,
               std::atomic<bool>& cancelled, size_t& sent, size_t total,
               const TranscriptionService::ProgressCallback& callback)
{
    size_t offset = 0;
    while (offset < size) {
        if (cancelled.load()) return false;
        const size_t chunk = std::min<size_t>(4096, size - offset);
        const int written = esp_http_client_write(client, reinterpret_cast<const char*>(data + offset), chunk);
        if (written <= 0) return false;
        offset += static_cast<size_t>(written);
        sent += static_cast<size_t>(written);
        if (callback) callback(total == 0 ? 0.0f : static_cast<float>(sent) / total, "Enviando");
    }
    return true;
}

}  // namespace

bool TranscriptionService::transcribe(const std::string& url, const AudioBuffer& audio,
                                      std::atomic<bool>& cancel_requested, std::string& text,
                                      std::string& error, const ProgressCallback& callback)
{
    text.clear();
    WavBuffer wav;
    if (!AudioService::make_wav(audio, wav, error)) return false;

    const std::string prefix =
        std::string("--") + kBoundary + "\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"dictado.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";
    const std::string suffix =
        std::string("\r\n--") + kBoundary + "\r\nContent-Disposition: form-data; name=\"language\"\r\n\r\nes\r\n" +
        "--" + kBoundary + "\r\nContent-Disposition: form-data; name=\"response_format\"\r\n\r\njson\r\n" +
        "--" + kBoundary + "\r\nContent-Disposition: form-data; name=\"temperature\"\r\n\r\n0.0\r\n" +
        "--" + kBoundary + "\r\nContent-Disposition: form-data; name=\"no_timestamps\"\r\n\r\ntrue\r\n" +
        "--" + kBoundary + "--\r\n";
    const size_t total = prefix.size() + wav.size + suffix.size();

    esp_http_client_config_t config {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    // Cinco segundos para conectar; tras abrir se amplía a 60 s para la respuesta.
    config.timeout_ms = 5000;
    config.buffer_size = 1024;
    config.buffer_size_tx = 4096;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        error = "No se pudo crear la solicitud HTTP";
        return false;
    }
    const std::string content_type = std::string("multipart/form-data; boundary=") + kBoundary;
    esp_http_client_set_header(client, "Content-Type", content_type.c_str());
    esp_http_client_set_header(client, "Accept", "application/json");
    if (callback) callback(0.0f, "Conectando");
    esp_err_t err = esp_http_client_open(client, static_cast<int>(total));
    if (err != ESP_OK) {
        error = std::string("No se pudo conectar con Whisper: ") + esp_err_to_name(err);
        esp_http_client_cleanup(client);
        return false;
    }
    esp_http_client_set_timeout_ms(client, 60000);

    size_t sent = 0;
    bool ok = write_all(client, reinterpret_cast<const uint8_t*>(prefix.data()), prefix.size(), cancel_requested,
                        sent, total, callback) &&
              write_all(client, wav.bytes.get(), wav.size, cancel_requested, sent, total, callback) &&
              write_all(client, reinterpret_cast<const uint8_t*>(suffix.data()), suffix.size(), cancel_requested,
                        sent, total, callback);
    wav.bytes.reset();
    wav.size = 0;
    if (!ok) {
        error = cancel_requested.load() ? "Transcripción cancelada" : "Falló el envío del audio";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    if (callback) callback(1.0f, "Transcribiendo");
    const int64_t content_length = esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        error = "Whisper respondió HTTP " + std::to_string(status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    if (content_length > static_cast<int64_t>(kMaxResponse)) {
        error = "La respuesta de Whisper supera 4 KB";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }
    char response[kMaxResponse + 1] = {};
    size_t used = 0;
    while (used < kMaxResponse && !cancel_requested.load()) {
        const int read = esp_http_client_read(client, response + used, kMaxResponse - used);
        if (read < 0) {
            error = "No se pudo leer la respuesta de Whisper";
            ok = false;
            break;
        }
        if (read == 0) break;
        used += static_cast<size_t>(read);
    }
    if (used == kMaxResponse && !cancel_requested.load()) {
        char extra = 0;
        const int extra_read = esp_http_client_read(client, &extra, 1);
        if (extra_read > 0) {
            error = "La respuesta de Whisper supera 4 KB";
            ok = false;
        } else if (extra_read < 0) {
            error = "No se pudo completar la lectura de Whisper";
            ok = false;
        }
    }
    response[used] = '\0';
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (!ok || cancel_requested.load()) {
        if (cancel_requested.load()) error = "Transcripción cancelada";
        return false;
    }

    return parse_response(response, used, text, error);
}

bool TranscriptionService::parse_response(const char* json, size_t length, std::string& text, std::string& error)
{
    text.clear();
    cJSON* root = json == nullptr ? nullptr : cJSON_ParseWithLength(json, length);
    cJSON* value = root == nullptr ? nullptr : cJSON_GetObjectItemCaseSensitive(root, "text");
    if (!cJSON_IsString(value) || value->valuestring == nullptr) {
        if (root != nullptr) cJSON_Delete(root);
        error = "Respuesta JSON inválida o sin el campo text";
        ESP_LOGW(TAG, "Respuesta de Whisper inválida (%u bytes)", static_cast<unsigned>(length));
        return false;
    }
    text = value->valuestring;
    cJSON_Delete(root);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r')) text.erase(0, 1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r')) text.pop_back();
    if (text.empty()) {
        error = "Whisper no detectó texto";
        return false;
    }
    error.clear();
    return true;
}

}  // namespace el_sistema
