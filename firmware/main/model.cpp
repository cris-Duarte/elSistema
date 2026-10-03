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

size_t utf8_codepoint_count(const std::string& text)
{
    size_t count = 0;
    for (const unsigned char byte : text) {
        if ((byte & 0xC0U) != 0x80U) ++count;
    }
    return count;
}

std::string utf8_truncate(const std::string& text, size_t max_codepoints)
{
    if (utf8_codepoint_count(text) <= max_codepoints) return text;
    size_t count = 0;
    size_t end = 0;
    while (end < text.size() && count < max_codepoints) {
        ++end;
        while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) ++end;
        ++count;
    }
    return text.substr(0, end);
}

}  // namespace el_sistema
