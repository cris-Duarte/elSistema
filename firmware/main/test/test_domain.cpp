#include "model.hpp"
#include "assistant.hpp"
#include "audio.hpp"
#include "audio_session.hpp"
#include "connectivity.hpp"
#include "services.hpp"
#include "transcription.hpp"

#include "unity.h"
#include "esp_heap_caps.h"

#include <cmath>
#include <cstring>

using namespace el_sistema;

TEST_CASE("el asistente recorre el ciclo visual sin IA", "[el_sistema][assistant]")
{
    AssistantService assistant;
    assistant.open(1000);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Greeting),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_FALSE(assistant.update(2799));
    TEST_ASSERT_TRUE(assistant.update(2800));
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Idle),
                      static_cast<int>(assistant.snapshot().state));

    assistant.start_visual_demo(3000);
    TEST_ASSERT_TRUE(assistant.snapshot().demo_running);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Listening),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_TRUE(assistant.update(6400));
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Thinking),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_TRUE(assistant.update(9000));
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Speaking),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_TRUE(assistant.update(12800));
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Idle),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_FALSE(assistant.snapshot().demo_running);
}

TEST_CASE("cerrar el asistente cancela su actividad visual", "[el_sistema][assistant]")
{
    AssistantService assistant;
    assistant.open(0);
    assistant.start_visual_demo(100);
    assistant.close(200);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Dormant),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_FALSE(assistant.snapshot().demo_running);
    assistant.tap(300);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Dormant),
                      static_cast<int>(assistant.snapshot().state));
}

TEST_CASE("los eventos de voz gobiernan estados visuales sin temporizador", "[el_sistema][assistant]")
{
    AssistantService assistant;
    assistant.open(0);
    assistant.begin_listening(100);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Listening),
                      static_cast<int>(assistant.snapshot().state));
    TEST_ASSERT_FALSE(assistant.update(100000));
    assistant.begin_thinking(100100);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Thinking),
                      static_cast<int>(assistant.snapshot().state));
    assistant.begin_speaking(100200);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Speaking),
                      static_cast<int>(assistant.snapshot().state));
    assistant.finish_turn(100300);
    TEST_ASSERT_EQUAL(static_cast<int>(AssistantVisualState::Idle),
                      static_cast<int>(assistant.snapshot().state));
}

TEST_CASE("XP aplica bases, minutos completos y límite temporal", "[el_sistema][xp]")
{
    TEST_ASSERT_EQUAL_UINT32(25, XpService::reward(MissionRank::E, 0));
    TEST_ASSERT_EQUAL_UINT32(120, XpService::reward(MissionRank::C, 10 * 60));
    TEST_ASSERT_EQUAL_UINT32(840, XpService::reward(MissionRank::S, 400 * 60));
}

TEST_CASE("una recompensa puede subir varios niveles", "[el_sistema][xp]")
{
    Profile profile;
    XpService::grant(profile, 1800);
    TEST_ASSERT_EQUAL_UINT32(4, profile.level);
    TEST_ASSERT_EQUAL_UINT32(0, profile.level_xp);
    TEST_ASSERT_EQUAL_UINT64(1800, profile.total_xp);
}

TEST_CASE("la búsqueda ignora mayúsculas y acentos españoles", "[el_sistema][search]")
{
    Mission mission;
    mission.title = "Revisión técnica";
    mission.description = "Diseño del módulo español";
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "revision"));
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "TECNICA"));
    TEST_ASSERT_TRUE(ApplicationService::matches_search(mission, "espanol"));
    TEST_ASSERT_FALSE(ApplicationService::matches_search(mission, "audio"));
}

TEST_CASE("el modelo localiza misiones por identificador estable", "[el_sistema][model]")
{
    AppState state;
    state.missions.push_back(Mission{.id = 7, .title = "Misión"});
    TEST_ASSERT_NOT_NULL(find_mission(state, 7));
    TEST_ASSERT_NULL(find_mission(state, 8));
}

TEST_CASE("los límites de texto cuentan caracteres UTF-8 y no bytes", "[el_sistema][utf8]")
{
    TEST_ASSERT_EQUAL_UINT32(8, utf8_codepoint_count("áéíóú ñü"));
    TEST_ASSERT_EQUAL_STRING("áéí", utf8_truncate("áéíóú", 3).c_str());
}

TEST_CASE("el FIR produce exactamente 16 kHz y conserva una señal audible", "[el_sistema][audio]")
{
    Downsampler48To16 downsampler;
    constexpr double pi = 3.14159265358979323846;
    size_t output_count = 0;
    size_t zero_crossings = 0;
    int16_t previous = 0;
    for (size_t index = 0; index < 48000; ++index) {
        const int16_t input = static_cast<int16_t>(12000.0 * std::sin(2.0 * pi * 3000.0 * index / 48000.0));
        int16_t output = 0;
        if (!downsampler.push(input, output)) continue;
        if (output_count > 32 && ((previous < 0 && output >= 0) || (previous >= 0 && output < 0))) ++zero_crossings;
        previous = output;
        ++output_count;
    }
    TEST_ASSERT_EQUAL_UINT32(16000, output_count);
    TEST_ASSERT_INT_WITHIN(20, 6000, zero_crossings);
}

TEST_CASE("WAV mono de 16 kHz contiene una cabecera y tamaño correctos", "[el_sistema][audio]")
{
    AudioBuffer audio;
    audio.sample_count = 160;
    audio.sample_rate = 16000;
    audio.samples.reset(static_cast<int16_t*>(heap_caps_calloc(audio.sample_count, sizeof(int16_t),
                                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    TEST_ASSERT_NOT_NULL(audio.samples.get());
    WavBuffer wav;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(AudioService::make_wav(audio, wav, error), error.c_str());
    TEST_ASSERT_EQUAL_UINT32(44 + 320, wav.size);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", wav.bytes.get(), 4);
    TEST_ASSERT_EQUAL_MEMORY("WAVE", wav.bytes.get() + 8, 4);
    TEST_ASSERT_EQUAL_MEMORY("data", wav.bytes.get() + 36, 4);
}

TEST_CASE("el árbitro de audio permite un único propietario", "[el_sistema][audio]")
{
    AudioSessionManager sessions;
    TEST_ASSERT_TRUE(sessions.try_acquire(AudioSessionOwner::DictationCapture));
    TEST_ASSERT_EQUAL(static_cast<int>(AudioSessionOwner::DictationCapture),
                      static_cast<int>(sessions.owner()));
    TEST_ASSERT_FALSE(sessions.try_acquire(AudioSessionOwner::AssistantPlayback));
    sessions.release(AudioSessionOwner::AssistantPlayback);
    TEST_ASSERT_TRUE(sessions.busy());
    sessions.release(AudioSessionOwner::DictationCapture);
    TEST_ASSERT_FALSE(sessions.busy());
    {
        AudioSessionLease lease(sessions, AudioSessionOwner::Reminder);
        TEST_ASSERT_TRUE(lease.acquired());
    }
    TEST_ASSERT_FALSE(sessions.busy());
}

TEST_CASE("el detector distingue voz calibrada y espera tres segundos de silencio", "[el_sistema][audio]")
{
    VoiceActivityDetector detector;
    VoiceDecision decision = VoiceDecision::Waiting;
    uint32_t elapsed = 0;
    for (; elapsed < 1000; elapsed += 20)
        decision = detector.update(0.00030f, 0.00120f, elapsed + 20);
    TEST_ASSERT_EQUAL(static_cast<int>(VoiceDecision::Waiting), static_cast<int>(decision));
    for (int chunk = 0; chunk < 3; ++chunk) {
        elapsed += 20;
        decision = detector.update(0.00150f, 0.00350f, elapsed);
    }
    TEST_ASSERT_TRUE(detector.voice_seen());
    TEST_ASSERT_EQUAL(static_cast<int>(VoiceDecision::Speaking), static_cast<int>(decision));
    for (int chunk = 0; chunk < 149; ++chunk) {
        elapsed += 20;
        decision = detector.update(0.00030f, 0.00120f, elapsed);
        TEST_ASSERT_NOT_EQUAL(static_cast<int>(VoiceDecision::StopForSilence), static_cast<int>(decision));
    }
    elapsed += 20;
    TEST_ASSERT_EQUAL(static_cast<int>(VoiceDecision::StopForSilence),
                      static_cast<int>(detector.update(0.00030f, 0.00120f, elapsed)));
}

TEST_CASE("el detector informa ausencia de voz tras cinco segundos", "[el_sistema][audio]")
{
    VoiceActivityDetector detector;
    VoiceDecision decision = VoiceDecision::Waiting;
    for (uint32_t elapsed = 20; elapsed <= 5000; elapsed += 20)
        decision = detector.update(0.00030f, 0.00120f, elapsed);
    TEST_ASSERT_FALSE(detector.voice_seen());
    TEST_ASSERT_EQUAL(static_cast<int>(VoiceDecision::StopForNoVoice), static_cast<int>(decision));
}

TEST_CASE("la respuesta de Whisper exige JSON con texto no vacío", "[el_sistema][transcription]")
{
    std::string text;
    std::string error;
    const char valid[] = "{\"text\":\"  misión con ñ  \"}";
    TEST_ASSERT_TRUE(TranscriptionService::parse_response(valid, std::strlen(valid), text, error));
    TEST_ASSERT_EQUAL_STRING("misión con ñ", text.c_str());
    const char invalid[] = "{\"result\":\"sin text\"}";
    TEST_ASSERT_FALSE(TranscriptionService::parse_response(invalid, std::strlen(invalid), text, error));
}

TEST_CASE("la configuración de conectividad conserva perfiles y servidor", "[el_sistema][connectivity]")
{
    NetworkConfigData source;
    source.profiles = {
        {"Casa Ñ", "contraseña-uno", 0, false},
        {"Red oculta", "secreto123", 1, true},
    };
    source.preferred_server_id = "b176e0a5-4570-4b38-a437-349d62d3260a";
    source.preferred_server_name = "MacBook de Cris";
    const std::string encoded = encode_network_config(source);
    TEST_ASSERT_FALSE(encoded.empty());
    NetworkConfigData decoded;
    TEST_ASSERT_TRUE(decode_network_config(encoded.c_str(), decoded));
    TEST_ASSERT_EQUAL_UINT32(2, decoded.profiles.size());
    TEST_ASSERT_EQUAL_STRING("Casa Ñ", decoded.profiles[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("contraseña-uno", decoded.profiles[0].password.c_str());
    TEST_ASSERT_TRUE(decoded.profiles[1].hidden);
    TEST_ASSERT_EQUAL_STRING(source.preferred_server_id.c_str(), decoded.preferred_server_id.c_str());
    TEST_ASSERT_FALSE(decode_network_config("{\"schema\":1,\"profiles\":[]}", decoded));
    TEST_ASSERT_FALSE(decode_network_config("json incompleto", decoded));
}

TEST_CASE("la migración antigua crea un único perfil y descarta la IP fija", "[el_sistema][connectivity]")
{
    const NetworkConfigData migrated = migrate_legacy_network_config("Casa", "secreto123");
    TEST_ASSERT_EQUAL_UINT32(1, migrated.profiles.size());
    TEST_ASSERT_EQUAL_STRING("Casa", migrated.profiles.front().ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("secreto123", migrated.profiles.front().password.c_str());
    TEST_ASSERT_TRUE(migrated.preferred_server_id.empty());
    TEST_ASSERT_TRUE(migrated.preferred_server_name.empty());
    TEST_ASSERT_TRUE(migrate_legacy_network_config("", "").profiles.empty());
}

TEST_CASE("la configuración limita de forma segura los perfiles a ocho", "[el_sistema][connectivity]")
{
    NetworkConfigData source;
    for (int index = 0; index < 10; ++index) {
        source.profiles.push_back({"Red " + std::to_string(index), "password" + std::to_string(index),
                                   static_cast<uint8_t>(index), false});
    }
    NetworkConfigData decoded;
    TEST_ASSERT_TRUE(decode_network_config(encode_network_config(source).c_str(), decoded));
    TEST_ASSERT_EQUAL_UINT32(ConnectivityService::kMaxProfiles, decoded.profiles.size());
}

TEST_CASE("los reintentos aplican 2 5 15 30 60 y luego 60 segundos", "[el_sistema][connectivity]")
{
    const uint32_t expected[] = {2, 5, 15, 30, 60, 60, 60};
    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); ++index)
        TEST_ASSERT_EQUAL_UINT32(expected[index], connectivity_retry_delay_seconds(index));
}

TEST_CASE("Bonjour deduplica por ID estable sin mezclar Macs", "[el_sistema][connectivity]")
{
    const std::vector<DiscoveredServer> input {
        {.id = "mac-a", .name = "Mac anterior", .ipv4 = "192.168.1.10", .port = 8080},
        {.id = "mac-a", .name = "Mac repetido", .ipv4 = "192.168.1.12", .port = 8080},
        {.id = "mac-b", .name = "Mac B", .ipv4 = "192.168.1.11", .port = 8080},
        {.id = "", .name = "Inválido", .ipv4 = "192.168.1.13", .port = 8080},
    };
    const std::vector<DiscoveredServer> unique = deduplicate_discovered_servers(input);
    TEST_ASSERT_EQUAL_UINT32(2, unique.size());
    TEST_ASSERT_EQUAL_STRING("mac-a", unique[0].id.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.10", unique[0].ipv4.c_str());
    TEST_ASSERT_EQUAL_STRING("mac-b", unique[1].id.c_str());
}

TEST_CASE("las redes se intentan por prioridad, señal y visibilidad", "[el_sistema][connectivity]")
{
    const std::vector<NetworkProfile> profiles {
        {"Prioritaria débil", "password1", 0, false},
        {"Secundaria fuerte", "password2", 1, false},
        {"Prioritaria fuerte", "password3", 0, false},
        {"Oculta prioritaria", "password4", 0, true},
        {"Fuera de alcance", "password5", 2, false},
    };
    const std::vector<WifiNetwork> networks {
        {"Prioritaria débil", -80, true, true},
        {"Secundaria fuerte", -35, true, true},
        {"Prioritaria fuerte", -45, true, true},
    };
    const std::vector<size_t> ranked = rank_network_candidates(profiles, networks);
    TEST_ASSERT_EQUAL_UINT32(4, ranked.size());
    TEST_ASSERT_EQUAL_UINT32(2, ranked[0]);
    TEST_ASSERT_EQUAL_UINT32(0, ranked[1]);
    TEST_ASSERT_EQUAL_UINT32(3, ranked[2]);
    TEST_ASSERT_EQUAL_UINT32(1, ranked[3]);
}

TEST_CASE("Bonjour respeta el Mac recordado y evita cambios silenciosos", "[el_sistema][connectivity]")
{
    const std::vector<DiscoveredServer> servers {
        {.id = "mac-a", .name = "Mac A", .ipv4 = "192.168.1.10", .port = 8080},
        {.id = "mac-b", .name = "Mac B", .ipv4 = "192.168.1.11", .port = 8080},
    };
    bool required = false;
    TEST_ASSERT_EQUAL_STRING("mac-b", choose_server_id("mac-b", servers, required).c_str());
    TEST_ASSERT_FALSE(required);
    TEST_ASSERT_TRUE(choose_server_id("mac-c", servers, required).empty());
    TEST_ASSERT_TRUE(required);
    TEST_ASSERT_TRUE(choose_server_id("", servers, required).empty());
    TEST_ASSERT_TRUE(required);
    TEST_ASSERT_EQUAL_STRING("mac-a", choose_server_id("", {servers.front()}, required).c_str());
    TEST_ASSERT_FALSE(required);
}
