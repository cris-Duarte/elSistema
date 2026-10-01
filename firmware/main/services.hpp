#pragma once

#include "model.hpp"

#include <string>
#include <vector>

namespace el_sistema {

class MissionRepository;
class ClockService;

class XpService {
public:
    static uint32_t reward(MissionRank rank, uint32_t active_seconds);
    static uint32_t required_for_level(uint32_t level);
    static void grant(Profile& profile, uint32_t amount);
};

class ApplicationService {
public:
    ApplicationService(AppState& state, MissionRepository& repository, ClockService& clock);

    void seed_first_run();
    uint32_t current_elapsed(const Mission& mission) const;

    MissionId create_mission(const std::string& title, const std::string& description, MissionRank rank,
                             bool reminder_enabled, uint16_t reminder_minutes);
    bool update_mission(MissionId id, const std::string& title, const std::string& description, MissionRank rank,
                        bool reminder_enabled, uint16_t reminder_minutes);
    bool delete_mission(MissionId id);

    bool add_to_focus(MissionId id);
    bool remove_from_focus(MissionId id);
    bool move_in_focus(MissionId id, int direction);
    bool start(MissionId id);
    bool pause_active();
    bool complete_active(uint32_t& awarded_xp, MissionId& suggested_next);

    std::vector<MissionId> due_reminders() const;
    bool resume_from_reminder(MissionId id);
    bool snooze_reminder(MissionId id, uint16_t minutes = 5);
    bool cancel_reminder(MissionId id);

    bool checkpoint();
    bool save();
    static bool matches_search(const Mission& mission, const std::string& query);

private:
    bool commit_interval(Mission& mission, bool start_reminder);
    bool persist_or_restore(const AppState& previous);
    void enforce_single_active();

    AppState& state_;
    MissionRepository& repository_;
    ClockService& clock_;
};

}  // namespace el_sistema
