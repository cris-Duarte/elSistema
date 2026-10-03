#include "services.hpp"

#include "hardware.hpp"
#include "repository.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "esp_timer.h"

namespace el_sistema {
namespace {

uint32_t rank_base(MissionRank rank)
{
    switch (rank) {
        case MissionRank::E: return 25;
        case MissionRank::D: return 50;
        case MissionRank::C: return 100;
        case MissionRank::B: return 200;
        case MissionRank::A: return 350;
        case MissionRank::S: return 600;
    }
    return 25;
}

void remove_id(std::vector<MissionId>& ids, MissionId id)
{
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}

std::string normalize_spanish(const std::string& text)
{
    std::string output;
    output.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        const unsigned char current = static_cast<unsigned char>(text[i]);
        if (current < 0x80) {
            output.push_back(static_cast<char>(std::tolower(current)));
            ++i;
            continue;
        }
        if (i + 1 < text.size() && current == 0xC3) {
            const unsigned char next = static_cast<unsigned char>(text[i + 1]);
            switch (next) {
                case 0x81: case 0xA1: output.push_back('a'); break;
                case 0x89: case 0xA9: output.push_back('e'); break;
                case 0x8D: case 0xAD: output.push_back('i'); break;
                case 0x93: case 0xB3: output.push_back('o'); break;
                case 0x9A: case 0xBA: case 0x9C: case 0xBC: output.push_back('u'); break;
                case 0x91: case 0xB1: output.push_back('n'); break;
                default:
                    output.push_back(static_cast<char>(current));
                    output.push_back(static_cast<char>(next));
                    break;
            }
            i += 2;
            continue;
        }
        output.push_back(static_cast<char>(current));
        ++i;
    }
    return output;
}

}  // namespace

uint32_t XpService::reward(MissionRank rank, uint32_t active_seconds)
{
    const uint32_t minutes = std::min<uint32_t>(active_seconds / 60, 120);
    return rank_base(rank) + minutes * 2;
}

uint32_t XpService::required_for_level(uint32_t level)
{
    return 500 + 100 * (std::max<uint32_t>(1, level) - 1);
}

void XpService::grant(Profile& profile, uint32_t amount)
{
    profile.total_xp += amount;
    profile.level_xp += amount;
    while (profile.level_xp >= required_for_level(profile.level)) {
        profile.level_xp -= required_for_level(profile.level);
        ++profile.level;
    }
}

ApplicationService::ApplicationService(AppState& state, MissionRepository& repository, ClockService& clock)
    : state_(state), repository_(repository), clock_(clock)
{
}

void ApplicationService::seed_first_run()
{
    if (!state_.missions.empty()) return;
    const int64_t now = clock_.now_utc();
    const struct Seed {
        const char* title;
        const char* description;
        MissionRank rank;
    } seeds[] = {
        {"Planear la siguiente función", "Divide el trabajo en un objetivo pequeño y verificable.", MissionRank::D},
        {"Revisar el build pendiente", "Aprovecha el tiempo de compilación sin perder el contexto.", MissionRank::C},
        {"Documentar una decisión técnica", "Registra el motivo antes de continuar con la implementación.", MissionRank::B},
    };
    for (const auto& seed : seeds) {
        Mission mission;
        mission.id = state_.next_mission_id++;
        mission.title = seed.title;
        mission.description = seed.description;
        mission.rank = seed.rank;
        mission.created_at_utc = now;
        mission.updated_at_utc = now;
        mission.reminder.delay_minutes = state_.settings.default_reminder_minutes;
        state_.missions.push_back(std::move(mission));
    }
    repository_.save(state_);
}

uint32_t ApplicationService::current_elapsed(const Mission& mission) const
{
    uint32_t elapsed = mission.elapsed_seconds;
    if (mission.status == MissionStatus::FocusActive && mission.runtime_started_us > 0) {
        const int64_t delta = esp_timer_get_time() - mission.runtime_started_us;
        if (delta > 0) elapsed += static_cast<uint32_t>(delta / 1000000);
    }
    return elapsed;
}

bool ApplicationService::persist_or_restore(const AppState& previous)
{
    if (repository_.save(state_)) return true;
    state_ = previous;
    return false;
}

MissionId ApplicationService::create_mission(const std::string& title, const std::string& description, MissionRank rank,
                                             bool reminder_enabled, uint16_t reminder_minutes)
{
    if (title.empty() || utf8_codepoint_count(title) > 80 || utf8_codepoint_count(description) > 240 ||
        repository_.storage_percent() >= 95) return 0;
    const AppState previous = state_;
    Mission mission;
    mission.id = state_.next_mission_id++;
    mission.title = title;
    mission.description = description;
    mission.rank = rank;
    mission.created_at_utc = clock_.now_utc();
    mission.updated_at_utc = mission.created_at_utc;
    mission.reminder.enabled = reminder_enabled;
    mission.reminder.delay_minutes = std::clamp<uint16_t>(reminder_minutes, 1, 120);
    state_.missions.push_back(std::move(mission));
    const MissionId id = state_.missions.back().id;
    return persist_or_restore(previous) ? id : 0;
}

bool ApplicationService::update_mission(MissionId id, const std::string& title, const std::string& description,
                                        MissionRank rank, bool reminder_enabled, uint16_t reminder_minutes)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr || mission->status == MissionStatus::Completed || title.empty() ||
        utf8_codepoint_count(title) > 80 || utf8_codepoint_count(description) > 240) return false;
    const AppState previous = state_;
    if (mission->status == MissionStatus::FocusActive) commit_interval(*mission, true);
    mission = find_mission(state_, id);
    mission->title = title;
    mission->description = description;
    mission->rank = rank;
    mission->reminder.enabled = reminder_enabled;
    mission->reminder.delay_minutes = std::clamp<uint16_t>(reminder_minutes, 1, 120);
    mission->updated_at_utc = clock_.now_utc();
    return persist_or_restore(previous);
}

bool ApplicationService::delete_mission(MissionId id)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr) return false;
    const AppState previous = state_;
    if (state_.focus.active_id == id) {
        commit_interval(*mission, false);
        state_.focus.active_id = 0;
    }
    remove_id(state_.focus.ordered_ids, id);
    state_.missions.erase(std::remove_if(state_.missions.begin(), state_.missions.end(),
                                         [id](const Mission& item) { return item.id == id; }),
                          state_.missions.end());
    return persist_or_restore(previous);
}

bool ApplicationService::add_to_focus(MissionId id)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr || mission->status == MissionStatus::Completed) return false;
    if (std::find(state_.focus.ordered_ids.begin(), state_.focus.ordered_ids.end(), id) != state_.focus.ordered_ids.end()) {
        return true;
    }
    const AppState previous = state_;
    state_.focus.ordered_ids.push_back(id);
    mission->status = MissionStatus::FocusPaused;
    mission->updated_at_utc = clock_.now_utc();
    return persist_or_restore(previous);
}

bool ApplicationService::remove_from_focus(MissionId id)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr || mission->status == MissionStatus::Completed) return false;
    const AppState previous = state_;
    if (state_.focus.active_id == id) {
        commit_interval(*mission, false);
        state_.focus.active_id = 0;
    }
    remove_id(state_.focus.ordered_ids, id);
    mission = find_mission(state_, id);
    mission->status = MissionStatus::Backlog;
    mission->reminder.due_at_utc = 0;
    mission->reminder.cancelled_for_pause = false;
    mission->updated_at_utc = clock_.now_utc();
    return persist_or_restore(previous);
}

bool ApplicationService::move_in_focus(MissionId id, int direction)
{
    auto it = std::find(state_.focus.ordered_ids.begin(), state_.focus.ordered_ids.end(), id);
    if (it == state_.focus.ordered_ids.end()) return false;
    const ptrdiff_t index = std::distance(state_.focus.ordered_ids.begin(), it);
    const ptrdiff_t next = index + (direction < 0 ? -1 : 1);
    if (next < 0 || next >= static_cast<ptrdiff_t>(state_.focus.ordered_ids.size())) return false;
    const AppState previous = state_;
    std::swap(state_.focus.ordered_ids[index], state_.focus.ordered_ids[next]);
    return persist_or_restore(previous);
}

bool ApplicationService::commit_interval(Mission& mission, bool start_reminder)
{
    if (mission.status != MissionStatus::FocusActive) return true;
    const int64_t now_us = esp_timer_get_time();
    const uint32_t delta = mission.runtime_started_us > 0 && now_us > mission.runtime_started_us
                               ? static_cast<uint32_t>((now_us - mission.runtime_started_us) / 1000000)
                               : 0;
    mission.elapsed_seconds += delta;
    ActiveInterval interval;
    interval.started_at_utc = mission.interval_started_utc;
    interval.ended_at_utc = clock_.now_utc();
    interval.duration_seconds = delta;
    if (delta > 0) mission.intervals.push_back(interval);
    mission.runtime_started_us = 0;
    mission.interval_started_utc = 0;
    mission.status = MissionStatus::FocusPaused;
    mission.updated_at_utc = interval.ended_at_utc;
    mission.reminder.cancelled_for_pause = false;
    mission.reminder.due_at_utc = start_reminder && mission.reminder.enabled
                                      ? interval.ended_at_utc + mission.reminder.delay_minutes * 60
                                      : 0;
    return true;
}

void ApplicationService::enforce_single_active()
{
    for (auto& mission : state_.missions) {
        if (mission.id != state_.focus.active_id && mission.status == MissionStatus::FocusActive) {
            commit_interval(mission, true);
        }
    }
}

bool ApplicationService::start(MissionId id)
{
    Mission* next = find_mission(state_, id);
    if (next == nullptr || next->status == MissionStatus::Completed) return false;
    const AppState previous = state_;
    if (state_.focus.active_id != 0 && state_.focus.active_id != id) {
        Mission* active = find_mission(state_, state_.focus.active_id);
        if (active != nullptr) commit_interval(*active, true);
    }
    if (std::find(state_.focus.ordered_ids.begin(), state_.focus.ordered_ids.end(), id) == state_.focus.ordered_ids.end()) {
        state_.focus.ordered_ids.push_back(id);
    }
    next = find_mission(state_, id);
    next->status = MissionStatus::FocusActive;
    next->runtime_started_us = esp_timer_get_time();
    next->interval_started_utc = clock_.now_utc();
    next->updated_at_utc = next->interval_started_utc;
    next->reminder.due_at_utc = 0;
    next->reminder.cancelled_for_pause = false;
    state_.focus.active_id = id;
    enforce_single_active();
    return persist_or_restore(previous);
}

bool ApplicationService::pause_active()
{
    Mission* mission = find_mission(state_, state_.focus.active_id);
    if (mission == nullptr) return false;
    const AppState previous = state_;
    commit_interval(*mission, true);
    state_.focus.active_id = 0;
    return persist_or_restore(previous);
}

bool ApplicationService::complete_active(uint32_t& awarded_xp, MissionId& suggested_next)
{
    Mission* mission = find_mission(state_, state_.focus.active_id);
    if (mission == nullptr) return false;
    const AppState previous = state_;
    const MissionId completed_id = mission->id;
    commit_interval(*mission, false);
    mission = find_mission(state_, completed_id);
    awarded_xp = XpService::reward(mission->rank, mission->elapsed_seconds);
    mission->xp_awarded = awarded_xp;
    mission->status = MissionStatus::Completed;
    mission->completed_at_utc = clock_.now_utc();
    mission->updated_at_utc = mission->completed_at_utc;
    mission->reminder.due_at_utc = 0;
    XpService::grant(state_.profile, awarded_xp);
    remove_id(state_.focus.ordered_ids, completed_id);
    state_.focus.active_id = 0;
    suggested_next = state_.focus.ordered_ids.empty() ? 0 : state_.focus.ordered_ids.front();
    return persist_or_restore(previous);
}

std::vector<MissionId> ApplicationService::due_reminders() const
{
    std::vector<MissionId> result;
    const int64_t now = clock_.now_utc();
    for (MissionId id : state_.focus.ordered_ids) {
        const Mission* mission = find_mission(state_, id);
        if (mission != nullptr && mission->status == MissionStatus::FocusPaused && mission->reminder.enabled &&
            !mission->reminder.cancelled_for_pause && mission->reminder.due_at_utc > 0 &&
            mission->reminder.due_at_utc <= now) {
            result.push_back(id);
        }
    }
    return result;
}

bool ApplicationService::resume_from_reminder(MissionId id)
{
    return start(id);
}

bool ApplicationService::snooze_reminder(MissionId id, uint16_t minutes)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr || mission->status != MissionStatus::FocusPaused) return false;
    const AppState previous = state_;
    mission->reminder.due_at_utc = clock_.now_utc() + std::max<uint16_t>(1, minutes) * 60;
    mission->reminder.cancelled_for_pause = false;
    return persist_or_restore(previous);
}

bool ApplicationService::cancel_reminder(MissionId id)
{
    Mission* mission = find_mission(state_, id);
    if (mission == nullptr) return false;
    const AppState previous = state_;
    mission->reminder.cancelled_for_pause = true;
    mission->reminder.due_at_utc = 0;
    return persist_or_restore(previous);
}

bool ApplicationService::checkpoint()
{
    const Mission* mission = find_mission(state_, state_.focus.active_id);
    if (mission == nullptr) return true;
    return repository_.save_checkpoint(mission->id, current_elapsed(*mission), mission->interval_started_utc);
}

bool ApplicationService::save()
{
    return repository_.save(state_);
}

bool ApplicationService::matches_search(const Mission& mission, const std::string& query)
{
    if (query.empty()) return true;
    const std::string needle = normalize_spanish(query);
    return normalize_spanish(mission.title).find(needle) != std::string::npos ||
           normalize_spanish(mission.description).find(needle) != std::string::npos;
}

}  // namespace el_sistema
