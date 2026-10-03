#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace el_sistema {

using MissionId = uint32_t;

enum class MissionRank : uint8_t { E, D, C, B, A, S };
enum class MissionStatus : uint8_t { Backlog, FocusPaused, FocusActive, Completed };

struct ActiveInterval {
    int64_t started_at_utc = 0;
    int64_t ended_at_utc = 0;
    uint32_t duration_seconds = 0;
};

struct PauseReminder {
    bool enabled = true;
    uint16_t delay_minutes = 5;
    int64_t due_at_utc = 0;
    bool cancelled_for_pause = false;
};

struct Mission {
    MissionId id = 0;
    std::string title;
    std::string description;
    MissionRank rank = MissionRank::E;
    MissionStatus status = MissionStatus::Backlog;
    std::vector<ActiveInterval> intervals;
    uint32_t elapsed_seconds = 0;
    int64_t runtime_started_us = 0;
    int64_t interval_started_utc = 0;
    PauseReminder reminder;
    int64_t created_at_utc = 0;
    int64_t updated_at_utc = 0;
    int64_t completed_at_utc = 0;
    uint32_t xp_awarded = 0;
};

struct FocusZone {
    std::vector<MissionId> ordered_ids;
    MissionId active_id = 0;
};

struct Profile {
    uint32_t level = 1;
    uint32_t level_xp = 0;
    uint64_t total_xp = 0;
};

struct Settings {
    int16_t utc_offset_minutes = -180;
    std::string timezone = "America/Asuncion";
    uint16_t default_reminder_minutes = 5;
    bool tutorial_completed = false;
};

struct AppState {
    static constexpr uint32_t kSchemaVersion = 1;

    uint32_t schema_version = kSchemaVersion;
    MissionId next_mission_id = 1;
    std::vector<Mission> missions;
    FocusZone focus;
    Profile profile;
    Settings settings;
};

const char* rank_to_string(MissionRank rank);
MissionRank rank_from_string(const char* value);
const char* status_to_string(MissionStatus status);
MissionStatus status_from_string(const char* value);

Mission* find_mission(AppState& state, MissionId id);
const Mission* find_mission(const AppState& state, MissionId id);

// Cuenta puntos de codigo UTF-8, no bytes. Los limites visibles de las
// misiones se expresan en caracteres y deben tratar á/ñ como uno solo.
size_t utf8_codepoint_count(const std::string& text);
std::string utf8_truncate(const std::string& text, size_t max_codepoints);

}  // namespace el_sistema
