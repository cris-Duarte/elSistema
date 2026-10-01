#include "model.hpp"

#include <cstring>

namespace el_sistema {

const char* rank_to_string(MissionRank rank)
{
    switch (rank) {
        case MissionRank::E: return "E";
        case MissionRank::D: return "D";
        case MissionRank::C: return "C";
        case MissionRank::B: return "B";
        case MissionRank::A: return "A";
        case MissionRank::S: return "S";
    }
    return "E";
}

MissionRank rank_from_string(const char* value)
{
    if (value == nullptr) return MissionRank::E;
    switch (value[0]) {
        case 'D': return MissionRank::D;
        case 'C': return MissionRank::C;
        case 'B': return MissionRank::B;
        case 'A': return MissionRank::A;
        case 'S': return MissionRank::S;
        default: return MissionRank::E;
    }
}

const char* status_to_string(MissionStatus status)
{
    switch (status) {
        case MissionStatus::Backlog: return "backlog";
        case MissionStatus::FocusPaused: return "focus-paused";
        case MissionStatus::FocusActive: return "focus-active";
        case MissionStatus::Completed: return "completed";
    }
    return "backlog";
}

MissionStatus status_from_string(const char* value)
{
    if (value == nullptr) return MissionStatus::Backlog;
    if (std::strcmp(value, "focus-paused") == 0) return MissionStatus::FocusPaused;
    if (std::strcmp(value, "focus-active") == 0) return MissionStatus::FocusActive;
    if (std::strcmp(value, "completed") == 0) return MissionStatus::Completed;
    return MissionStatus::Backlog;
}

Mission* find_mission(AppState& state, MissionId id)
{
    for (auto& mission : state.missions) {
        if (mission.id == id) return &mission;
    }
    return nullptr;
}

const Mission* find_mission(const AppState& state, MissionId id)
{
    for (const auto& mission : state.missions) {
        if (mission.id == id) return &mission;
    }
    return nullptr;
}

}  // namespace el_sistema
