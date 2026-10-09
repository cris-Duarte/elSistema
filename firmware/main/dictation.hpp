#pragma once

#include "audio.hpp"
#include "audio_session.hpp"
#include "ble.hpp"
#include "connectivity.hpp"
#include "transcription.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace el_sistema {

enum class DictationPhase : uint8_t {
    Unconfigured,
    Idle,
    Preparing,
    Recording,
    Processing,
    Connecting,
    Uploading,
    Transcribing,
    LocalResult,
    Result,
    Error,
};

struct DictationSnapshot {
    DictationPhase phase = DictationPhase::Unconfigured;
    uint32_t recorded_ms = 0;
    float level = 0.0f;
    float peak = 0.0f;
    std::array<float, 4> slot_levels {};
    std::array<float, 4> slot_peaks {};
    std::array<int16_t, 4> slot_min {};
    std::array<int16_t, 4> slot_max {};
    std::array<uint16_t, 4> slot_nonzero {};
    uint32_t bytes_read = 0;
    uint8_t microphone_slot_a = 0;
    uint8_t microphone_slot_b = 1;
    uint8_t reference_slot = 2;
    float progress = 0.0f;
    bool has_audio = false;
    std::string message;
    std::string candidate;
    std::string error;
};

struct RadarWifiBatch {
    uint32_t generation = 0, session = 0;
    std::vector<RadarObservation> observations;
    std::string error;
};

class DictationService {
public:
    DictationService(AudioService& audio, AudioSessionManager& audio_sessions,
                     ConnectivityService& connectivity, BleService& ble,
                     TranscriptionService& transcription);
    bool init();
    DictationSnapshot snapshot() const;

    void start(AudioSessionOwner owner = AudioSessionOwner::DictationCapture);
    void stop();
    void cancel();
    void retry();
    void start_connectivity();
    void scan_networks();
    void reconnect_networks();
    bool connect_and_save(const NetworkCredentials& credentials);
    void connect_known(size_t index);
    void forget_network(size_t index);
    void move_network(size_t index, int delta);
    void discover_servers();
    bool select_server(const std::string& id);
    void test_server();
    ConnectivitySnapshot connectivity_snapshot() const { return connectivity_.snapshot(); }
    BleSnapshot ble_snapshot() const { return ble_.snapshot(); }
    std::string accept_result();
    void discard();

    bool busy() const;
    bool has_audio() const { return snapshot().has_audio; }
    bool play_last_capture();
    void radar_session(uint32_t session);
    bool request_radar_scan(uint32_t session);
    RadarWifiBatch radar_wifi_batch(uint32_t after_generation = 0) const;
    // Estado de red para la barra superior: no inician el Wi-Fi al consultar.
    bool wifi_configured() const { return connectivity_.configured(); }
    bool wifi_connected() const { return connectivity_.connected(); }
    bool transcription_ready() const { return connectivity_.ready(); }

private:
    enum class Command : uint8_t {
        Start,
        Retry,
        StartConnectivity,
        Scan,
        Reconnect,
        ConnectAndSave,
        ConnectKnown,
        ForgetNetwork,
        MoveNetwork,
        DiscoverServers,
        SelectServer,
        TestServer,
        Play,
    };
    struct CommandMessage {
        Command command = Command::Start;
        int16_t index = -1;
        int8_t delta = 0;
        AudioSessionOwner audio_owner = AudioSessionOwner::None;
    };
    static void task_entry(void* arg);
    void run();
    void handle(const CommandMessage& command);
    bool send(Command command, int16_t index = -1, int8_t delta = 0,
              AudioSessionOwner audio_owner = AudioSessionOwner::None);
    void record_and_transcribe(AudioSessionOwner owner);
    void transcribe_current();
    void reset_to_idle();
    void update(const DictationSnapshot& snapshot);
    void set_phase(DictationPhase phase, const std::string& message = {});

    AudioService& audio_;
    AudioSessionManager& audio_sessions_;
    ConnectivityService& connectivity_;
    BleService& ble_;
    TranscriptionService& transcription_;
    mutable SemaphoreHandle_t mutex_ = nullptr;
    QueueHandle_t commands_ = nullptr;
    TaskHandle_t task_ = nullptr;
    DictationSnapshot snapshot_;
    std::atomic<uint32_t> radar_session_ {0}, radar_requested_ {0};
    RadarWifiBatch radar_batch_;
    AudioBuffer audio_buffer_;
    NetworkCredentials pending_credentials_;
    std::string pending_server_id_;
    std::atomic<bool> stop_requested_ {false};
    std::atomic<bool> cancel_requested_ {false};
};

const char* dictation_phase_name(DictationPhase phase);

}  // namespace el_sistema
