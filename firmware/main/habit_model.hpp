#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace el_sistema {
using HabitId = uint32_t;
using CivilDay = int32_t; // Days since 1970-01-01, deliberately independent of timezone.
enum class HabitKind : uint8_t { Daily, Continuous };
enum class HabitNoticeMode : uint8_t { Visual, Tone, Voice };
struct HabitGoal {
    CivilDay week = 0;
    uint8_t days = 5;
};
struct HabitPeriod {
    int64_t start = 0, end = 0;
}; // end=0 is open
struct HabitNoticeDay {
    CivilDay day = 0;
    bool announced = false, omitted = false;
    int64_t snoozed_until = 0;
};
struct Habit {
    HabitId id = 0;
    std::string name, description;
    HabitKind kind = HabitKind::Daily;
    CivilDay start_day = 0;
    std::vector<CivilDay> done;
    std::vector<HabitGoal> goals;
    std::vector<HabitPeriod> periods;
    bool notice_enabled = false;
    uint16_t notice_minute = 18 * 60;
    HabitNoticeMode notice_mode = HabitNoticeMode::Visual;
    std::vector<HabitNoticeDay> notices;
};
struct HabitDocument {
    uint32_t schema_version = 1;
    HabitId next_id = 1;
    uint8_t volume = 30;
    std::vector<Habit> habits;
};
struct HabitContext {
    int64_t utc = 0;
    int offset_minutes = -180;
    bool valid = false;
};
struct HabitSnapshot {
    HabitDocument document;
    uint32_t generation = 0, revision = 0, result_sequence = 0, notice_sequence = 0;
    CivilDay presentation_day = 0;
    std::vector<HabitId> presentation_ids;
    HabitId result_id = 0;
    bool available = false, read_only = false, storage_warning = false, result_ok = false;
    std::string error, result_error;
    std::vector<HabitId> pending, unannounced;
    HabitNoticeMode presentation_mode = HabitNoticeMode::Visual;
};
enum class HabitOperation : uint8_t {
    Save,
    Delete,
    ToggleDay,
    Goal,
    Interrupt,
    Resume,
    CorrectPeriod,
    Snooze,
    Omit,
    Announce,
    Volume,
    TestAudio,
    AudioError
};
struct HabitCommand {
    HabitOperation operation = HabitOperation::Save;
    uint32_t sequence = 0;
    HabitId id = 0;
    Habit habit;
    CivilDay day = 0;
    int value = 0;
    int64_t start = 0, end = 0;
    std::vector<HabitId> ids;
    HabitContext context;
};
CivilDay civil_day(int64_t utc, int offset);
CivilDay week_start(CivilDay day);
CivilDay date_day(int year, int month, int day);
std::string day_text(CivilDay day);
std::string datetime_text(int64_t utc, int offset);
bool parse_day(const std::string &text, CivilDay &day);
bool parse_datetime(const std::string &text, int offset, int64_t &utc);
const Habit *find_habit(const HabitDocument &document, HabitId id);
uint8_t weekly_goal(const Habit &habit, CivilDay day);
unsigned weekly_done(const Habit &habit, CivilDay day);
bool has_done(const Habit &habit, CivilDay day);
bool habit_clock_valid(const Habit &habit, HabitContext context);
int64_t period_seconds(const HabitPeriod &period, HabitContext context);
int64_t best_period_seconds(const Habit &habit, HabitContext context);
std::string habit_duration(int64_t seconds);
bool validate_habits(const HabitDocument &document, std::string &error);
bool apply_habit_command(HabitDocument &document, const HabitCommand &command, HabitId &result,
                         std::string &error);
std::vector<HabitId> pending_habits(const HabitDocument &document, HabitContext context, bool unannounced);
HabitNoticeMode notice_mode_for(const HabitDocument &document, const std::vector<HabitId> &ids);
} // namespace el_sistema
