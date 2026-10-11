#include "habit_model.hpp"
#include "habit_repository.hpp"
#include "modules.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace el_sistema;
namespace {
int assertions = 0;
void check(bool condition) {
    ++assertions;
    assert(condition);
}
int64_t utc(const char *text) {
    int64_t t = 0;
    check(parse_datetime(text, -180, t));
    return t;
}
HabitContext now{0, -180, true};
bool apply(HabitDocument &doc, HabitCommand command, HabitId *id = nullptr) {
    auto candidate = doc;
    command.context = now;
    HabitId result = 0;
    std::string error;
    const bool ok = apply_habit_command(candidate, command, result, error);
    if (ok)
        doc = std::move(candidate);
    if (id)
        *id = result;
    return ok;
}
HabitId create(HabitDocument &doc, HabitKind kind, const char *name = "Ejercicio", int goal = 5) {
    HabitCommand command;
    command.habit.name = name;
    command.habit.description = "Ánimo, ñ y ü";
    command.habit.kind = kind;
    command.habit.start_day = date_day(2026, 10, 7);
    command.value = goal;
    command.start = now.utc - 14 * 86400 - 123;
    HabitId id = 0;
    check(apply(doc, command, &id));
    return id;
}
void calendar() {
    check(static_cast<int>(ModuleId::Habits) == 6 && kModules.size() == 4);
    for (int y = 1970; y <= 2199; ++y)
        for (int month = 1; month <= 12; ++month) {
            auto d = date_day(y, month, 1);
            CivilDay parsed = 0;
            check(parse_day(day_text(d), parsed) && parsed == d);
            check(week_start(d) <= d && d - week_start(d) < 7);
            check(week_start(week_start(d) + 6) == week_start(d));
        }
    CivilDay d;
    check(parse_day("2024-02-29", d));
    check(!parse_day("2023-02-29", d));
    check(!parse_day("2026-04-31", d));
    check(!parse_day("2026-1-09", d));
    check(!parse_day("2026-10-09x", d));
    int64_t t;
    check(!parse_datetime("2026-10-09 24:00", -180, t));
    check(!parse_datetime("2026-10-09 20:01:60", -180, t));
    check(civil_day(utc("2026-10-09 00:00:00"), -180) == date_day(2026, 10, 9));
    check(week_start(date_day(2026, 10, 11)) == date_day(2026, 10, 5));
    check(datetime_text(utc("2026-10-09 19:30:04"), -180) == "2026-10-09 19:30:04");
}
void daily() {
    HabitDocument doc;
    auto id = create(doc, HabitKind::Daily, "Ejercicio", 1);
    HabitCommand mark;
    mark.operation = HabitOperation::ToggleDay;
    mark.id = id;
    mark.day = date_day(2026, 10, 7);
    check(apply(doc, mark));
    check(weekly_done(*find_habit(doc, id), mark.day) == 1);
    check(apply(doc, mark));
    check(weekly_done(*find_habit(doc, id), mark.day) == 0);
    mark.day = date_day(2026, 10, 6);
    check(!apply(doc, mark));
    mark.day = date_day(2026, 10, 10);
    check(!apply(doc, mark));
    HabitCommand goal;
    goal.operation = HabitOperation::Goal;
    goal.id = id;
    goal.day = date_day(2026, 10, 12);
    goal.value = 7;
    check(apply(doc, goal));
    check(weekly_goal(*find_habit(doc, id), date_day(2026, 10, 9)) == 1);
    check(weekly_goal(*find_habit(doc, id), date_day(2026, 10, 12)) == 7);
    goal.day = date_day(2026, 10, 5);
    goal.value = 5;
    check(apply(doc, goal));
    check(weekly_goal(*find_habit(doc, id), date_day(2026, 10, 9)) == 5);
    check(weekly_goal(*find_habit(doc, id), date_day(2026, 10, 12)) == 7);
    goal.value = 0;
    check(!apply(doc, goal));
    goal.value = 8;
    check(!apply(doc, goal));
    goal.value = 5;
    goal.day = date_day(2026, 9, 28);
    check(!apply(doc, goal));
    mark.day = date_day(2026, 10, 9);
    check(apply(doc, mark));
    HabitCommand edit;
    edit.id = id;
    edit.habit = *find_habit(doc, id);
    edit.habit.kind = HabitKind::Continuous;
    edit.start = now.utc - 100;
    check(!apply(doc, edit));
    edit.habit = *find_habit(doc, id);
    edit.habit.name = "  ";
    check(!apply(doc, edit));
    edit.habit.name = std::string(81, 'A');
    check(!apply(doc, edit));
    edit.habit.name.clear();
    for (int i = 0; i < 80; ++i)
        edit.habit.name += "ñ";
    check(apply(doc, edit));
    edit.habit.name += "é";
    check(!apply(doc, edit));
    edit.habit.name = "válido";
    edit.habit.description = std::string(241, 'a');
    check(!apply(doc, edit));
    edit.habit.description = std::string("\xC0\xAF", 2);
    check(!apply(doc, edit));
    check(has_done(*find_habit(doc, id), date_day(2026, 10, 9))); // Civil history doesn't shift with timezone.
    HabitContext invalid = now;
    invalid.valid = false;
    check(pending_habits(doc, invalid, false).empty());
}
void periods() {
    HabitDocument doc;
    auto id = create(doc, HabitKind::Continuous, "Sin fumar");
    const int64_t initial = find_habit(doc, id)->periods.front().start;
    check(period_seconds(find_habit(doc, id)->periods.back(), now) == 14 * 86400 + 123);
    HabitCommand stop;
    stop.operation = HabitOperation::Interrupt;
    stop.id = id;
    stop.end = now.utc + 1;
    check(!apply(doc, stop));
    stop.end = initial - 1;
    check(!apply(doc, stop));
    stop.end = now.utc;
    check(apply(doc, stop));
    const auto frozen = best_period_seconds(*find_habit(doc, id), now);
    const auto saved_now = now;
    now.utc += 86400;
    check(period_seconds(find_habit(doc, id)->periods.back(), now) == frozen);
    HabitCommand resume;
    resume.operation = HabitOperation::Resume;
    resume.id = id;
    resume.start = now.utc - 86401;
    check(!apply(doc, resume));
    resume.start = now.utc - 3600;
    check(apply(doc, resume));
    check(!apply(doc, resume));
    check(find_habit(doc, id)->periods.size() == 2);
    check(best_period_seconds(*find_habit(doc, id), now) == frozen);
    HabitCommand correct;
    correct.operation = HabitOperation::CorrectPeriod;
    correct.id = id;
    correct.value = 1;
    correct.start = initial;
    check(!apply(doc, correct));
    correct.start = now.utc - 1800;
    check(apply(doc, correct));
    correct.value = 0;
    correct.start = initial - 10;
    correct.end = now.utc - 1799;
    check(!apply(doc, correct));
    correct.end = saved_now.utc;
    check(apply(doc, correct));
    HabitContext invalid = now;
    invalid.valid = false;
    check(best_period_seconds(*find_habit(doc, id), invalid) == -1);
    invalid = now;
    invalid.utc = initial - 1;
    check(!habit_clock_valid(*find_habit(doc, id), invalid));
    now.utc += 5 * 86400;
    check(period_seconds(find_habit(doc, id)->periods.back(), now) ==
          5 * 86400 + 1800); // Power-off time included.
    now = saved_now;
}
void notices() {
    HabitDocument doc;
    auto a = create(doc, HabitKind::Daily), b = create(doc, HabitKind::Daily, "Leer");
    for (auto &h : doc.habits) {
        h.notice_enabled = true;
        h.notice_mode = h.id == a ? HabitNoticeMode::Tone : HabitNoticeMode::Voice;
    }
    check(pending_habits(doc, now, true).size() == 2);
    check(notice_mode_for(doc, {a, b}) == HabitNoticeMode::Voice);
    HabitCommand receipt;
    receipt.operation = HabitOperation::Announce;
    receipt.ids = {a, b};
    check(apply(doc, receipt));
    check(pending_habits(doc, now, true).empty());
    check(pending_habits(doc, now, false).size() == 2);
    std::string json, error;
    check(habit_json_encode(doc, json, error));
    HabitDocument reboot;
    check(habit_json_decode(json, reboot, error));
    check(pending_habits(reboot, now, true).empty());
    HabitCommand snooze;
    snooze.operation = HabitOperation::Snooze;
    snooze.id = a;
    check(apply(doc, snooze));
    check(pending_habits(doc, now, false).size() == 1);
    now.utc += 899;
    check(pending_habits(doc, now, true).empty());
    now.utc += 1;
    check(pending_habits(doc, now, true) == std::vector<HabitId>{a});
    check(apply(doc, receipt));
    HabitCommand omit;
    omit.operation = HabitOperation::Omit;
    omit.id = a;
    check(apply(doc, omit));
    check(find_habit(doc, a)->done.empty());
    HabitCommand mark;
    mark.operation = HabitOperation::ToggleDay;
    mark.id = b;
    mark.day = civil_day(now.utc, now.offset_minutes);
    check(apply(doc, mark));
    check(pending_habits(doc, now, false).empty());
    check(apply(doc, mark));
    check(pending_habits(doc, now, true).empty());
    auto before = now;
    now.utc = utc("2026-10-10 17:59:00");
    check(pending_habits(doc, now, false).empty());
    now.utc += 60;
    check(pending_habits(doc, now, true).size() == 2);
    now = before;
    check(pending_habits(doc, now, true).empty()); // Clock correction doesn't duplicate receipt.
    HabitContext invalid = now;
    invalid.valid = false;
    check(pending_habits(doc, invalid, true).empty());
    doc.habits[1].goals = {{week_start(doc.habits[1].start_day), 1}};
    doc.habits[1].done = {date_day(2026, 10, 7)};
    check(pending_habits(doc, now, false).empty());
    now.utc = utc("2026-10-09 19:00:00");
}
void storage() {
    char path[] = "/private/tmp/elsistema-habit-tests.XXXXXX";
    check(mkdtemp(path));
    const std::string dir = path;
    HabitRepository repository(dir);
    HabitDocument doc;
    std::string error;
    check(repository.load(doc, error) && doc.habits.empty());
    auto id = create(doc, HabitKind::Daily);
    check(repository.save(doc, error));
    auto previous = doc;
    doc.habits[0].name = "Nueva versión";
    check(repository.save(doc, error));
    std::string json;
    check(habit_json_encode(doc, json, error));
    HabitDocument decoded;
    check(habit_json_decode(json, decoded, error));
    check(decoded.habits[0].name == doc.habits[0].name && decoded.next_id == doc.next_id);
    check(!habit_json_decode(json + "garbage", decoded, error));
    check(!habit_json_decode("{\"schema_version\":2}", decoded, error));
    check(!habit_json_decode(json.substr(0, json.size() - 5), decoded, error));
    {
        std::ofstream f(dir + "/habits.json");
        f << "{broken";
    }
    HabitRepository recovery(dir);
    HabitDocument restored;
    check(recovery.load(restored, error));
    check(restored.habits[0].name == previous.habits[0].name);
    // Failed temporary creation must not publish candidate data or damage backup.
    std::filesystem::create_directory(dir + "/habits.tmp");
    check(!recovery.save(doc, error));
    check(recovery.load(restored, error));
    check(restored.habits[0].name == previous.habits[0].name);
    std::filesystem::remove(dir + "/habits.tmp");
    check(recovery.save(doc, error));
    {
        std::ofstream f(dir + "/habits.json");
        f << "bad";
    }
    check(recovery.load(restored, error));
    check(restored.habits[0].name == previous.habits[0].name); // Valid backup survived recovery write.
    {
        std::ofstream f(dir + "/habits.bak");
        f << "bad too";
    }
    check(!recovery.load(restored, error));
    check(std::filesystem::exists(dir + "/habits.json"));
    // Only these explicitly-created test files are removed.
    for (const char *name : {"habits.json", "habits.bak", "habits.tmp"})
        std::filesystem::remove(dir + "/" + name);
    std::filesystem::remove(dir);
    check(id == 1);
}
} // namespace
int main() {
    now.utc = utc("2026-10-09 19:00:00");
    calendar();
    daily();
    periods();
    notices();
    storage();
    std::printf("Hábitos: %d comprobaciones de calendario, metas, UTF-8, periodos, avisos y respaldo OK\n",
                assertions);
}
