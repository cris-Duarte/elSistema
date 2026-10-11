#include "habit_model.hpp"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>

namespace el_sistema {
namespace {
int64_t floor_div(int64_t n, int64_t d) {
    return n / d - (n % d < 0);
}
bool utf8_length(const std::string &s, size_t max, bool required) {
    size_t count = 0;
    bool visible = false;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = static_cast<uint8_t>(s[i++]);
        unsigned trailing = 0;
        uint32_t minimum = 0;
        if (cp >= 0xC2 && cp <= 0xDF) {
            cp &= 31;
            trailing = 1;
            minimum = 0x80;
        } else if (cp >= 0xE0 && cp <= 0xEF) {
            cp &= 15;
            trailing = 2;
            minimum = 0x800;
        } else if (cp >= 0xF0 && cp <= 0xF4) {
            cp &= 7;
            trailing = 3;
            minimum = 0x10000;
        } else if (cp >= 0x80 || cp == 0)
            return false;
        for (unsigned j = 0; j < trailing; ++j) {
            if (i == s.size())
                return false;
            uint8_t b = s[i++];
            if ((b & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (b & 63);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        visible |= cp > 32;
        if (++count > max)
            return false;
    }
    return !required || visible;
}
bool legal_day(CivilDay d) {
    return d >= date_day(1, 1, 1) && d <= date_day(9999, 12, 31);
}
bool legal_utc(int64_t t) {
    return t >= -62135596800LL && t <= 253402300799LL;
}
HabitNoticeDay *notice_for(Habit &h, CivilDay day) {
    for (auto &n : h.notices)
        if (n.day == day)
            return &n;
    h.notices.push_back({day});
    return &h.notices.back();
}
const HabitNoticeDay *notice_for(const Habit &h, CivilDay day) {
    for (auto &n : h.notices)
        if (n.day == day)
            return &n;
    return nullptr;
}
} // namespace
CivilDay date_day(int y, int m, int d) {
    y -= m <= 2;
    const int era = static_cast<int>(floor_div(y, 400));
    const unsigned yo = y - era * 400;
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yo * 365 + yo / 4 - yo / 100 + doy - 719468;
}
CivilDay civil_day(int64_t utc, int offset) {
    return static_cast<CivilDay>(floor_div(utc + offset * 60LL, 86400));
}
CivilDay week_start(CivilDay d) {
    const int mod = ((d + 3) % 7 + 7) % 7;
    return d - mod;
}
std::string day_text(CivilDay day) {
    int z = day + 719468;
    const int era = static_cast<int>(floor_div(z, 146097));
    const unsigned doe = z - era * 146097;
    const unsigned yo = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = yo + era * 400;
    const unsigned doy = doe - (365 * yo + yo / 4 - yo / 100);
    const unsigned mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
    const int m = mp + (mp < 10 ? 3 : -9);
    y += m <= 2;
    char out[48];
    std::snprintf(out, sizeof(out), "%04d-%02d-%02u", y, m, d);
    return out;
}
std::string datetime_text(int64_t utc, int offset) {
    const int64_t local = utc + offset * 60LL;
    const int seconds = static_cast<int>(local - floor_div(local, 86400) * 86400);
    char time[48];
    std::snprintf(time, sizeof(time), " %02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    return day_text(civil_day(utc, offset)) + time;
}
bool parse_day(const std::string &t, CivilDay &day) {
    if (t.size() != 10 || t[4] != '-' || t[7] != '-')
        return false;
    for (size_t i = 0; i < t.size(); ++i)
        if (i != 4 && i != 7 && (t[i] < '0' || t[i] > '9'))
            return false;
    int y = 0, m = 0, d = 0;
    if (std::sscanf(t.c_str(), "%d-%d-%d", &y, &m, &d) != 3)
        return false;
    if (y < 1 || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    day = date_day(y, m, d);
    return day_text(day) == t;
}
bool parse_datetime(const std::string &t, int offset, int64_t &utc) {
    CivilDay day;
    if ((t.size() != 16 && t.size() != 19) || t[10] != ' ' || t[13] != ':' || !parse_day(t.substr(0, 10), day))
        return false;
    for (size_t i : {11U, 12U, 14U, 15U})
        if (t[i] < '0' || t[i] > '9')
            return false;
    int hour = (t[11] - '0') * 10 + t[12] - '0', minute = (t[14] - '0') * 10 + t[15] - '0';
    int second = 0;
    if (t.size() == 19) {
        if (t[16] != ':' || t[17] < '0' || t[17] > '9' || t[18] < '0' || t[18] > '9')
            return false;
        second = (t[17] - '0') * 10 + t[18] - '0';
    }
    if (hour > 23 || minute > 59 || second > 59)
        return false;
    utc = day * 86400LL + hour * 3600 + minute * 60 + second - offset * 60LL;
    return legal_utc(utc);
}
const Habit *find_habit(const HabitDocument &doc, HabitId id) {
    for (const auto &h : doc.habits)
        if (h.id == id)
            return &h;
    return nullptr;
}
bool has_done(const Habit &h, CivilDay day) {
    return std::binary_search(h.done.begin(), h.done.end(), day);
}
uint8_t weekly_goal(const Habit &h, CivilDay day) {
    uint8_t goal = 5;
    const CivilDay week = week_start(day);
    for (const auto &g : h.goals) {
        if (g.week > week)
            break;
        goal = g.days;
    }
    return goal;
}
unsigned weekly_done(const Habit &h, CivilDay day) {
    const auto week = week_start(day);
    return static_cast<unsigned>(
        std::count_if(h.done.begin(), h.done.end(), [week](CivilDay d) { return d >= week && d < week + 7; }));
}
bool habit_clock_valid(const Habit &h, HabitContext c) {
    if (!c.valid || !legal_utc(c.utc))
        return false;
    if (h.kind == HabitKind::Daily)
        return h.start_day <= civil_day(c.utc, c.offset_minutes);
    for (const auto &p : h.periods)
        if (p.start > c.utc || p.end > c.utc)
            return false;
    return true;
}
int64_t period_seconds(const HabitPeriod &p, HabitContext c) {
    if (!c.valid || p.start > c.utc || p.end > c.utc)
        return -1;
    return (p.end ? p.end : c.utc) - p.start;
}
int64_t best_period_seconds(const Habit &h, HabitContext c) {
    if (!habit_clock_valid(h, c))
        return -1;
    int64_t best = 0;
    for (const auto &p : h.periods)
        best = std::max(best, period_seconds(p, c));
    return best;
}
std::string habit_duration(int64_t s) {
    if (s < 0)
        return "Revisar fecha y hora";
    char out[96];
    std::snprintf(out, sizeof(out), "%lld sem · %lld d\n%02lld:%02lld:%02lld", static_cast<long long>(s / 604800),
                  static_cast<long long>((s / 86400) % 7), static_cast<long long>((s / 3600) % 24),
                  static_cast<long long>((s / 60) % 60), static_cast<long long>(s % 60));
    return out;
}
bool validate_habits(const HabitDocument &doc, std::string &error) {
    auto fail = [&](const char *e) {
        error = e;
        return false;
    };
    if (doc.schema_version != 1 || !doc.next_id || doc.volume > 100)
        return fail("Formato de hábitos no compatible");
    std::set<HabitId> ids;
    for (const auto &h : doc.habits) {
        if (!h.id || h.id >= doc.next_id || !ids.insert(h.id).second)
            return fail("Identificadores de hábitos inválidos");
        if (!utf8_length(h.name, 80, true))
            return fail("El nombre requiere 1-80 caracteres UTF-8");
        if (!utf8_length(h.description, 240, false))
            return fail("La descripción admite hasta 240 caracteres UTF-8");
        if (static_cast<unsigned>(h.kind) > 1 || static_cast<unsigned>(h.notice_mode) > 2 ||
            h.notice_minute >= 1440 || !legal_day(h.start_day))
            return fail("Configuración de hábito inválida");
        if (h.kind == HabitKind::Daily && !h.periods.empty())
            return fail("Un hábito diario no admite periodos");
        if (h.kind == HabitKind::Continuous && (!h.done.empty() || !h.goals.empty() || h.notice_enabled))
            return fail("Un hábito continuo no admite registros diarios");
        CivilDay last = std::numeric_limits<CivilDay>::min();
        for (auto d : h.done) {
            if (!legal_day(d) || d < h.start_day || d <= last)
                return fail("Registro diario inválido");
            last = d;
        }
        last = std::numeric_limits<CivilDay>::min();
        for (auto g : h.goals) {
            if (g.week < date_day(1, 1, 1) - 6 || g.week > date_day(9999, 12, 31) ||
                g.week != week_start(g.week) || g.week <= last || g.days < 1 || g.days > 7)
                return fail("Meta semanal inválida");
            last = g.week;
        }
        if (h.kind == HabitKind::Daily && h.goals.empty())
            return fail("Falta la meta semanal");
        int64_t previous_end = std::numeric_limits<int64_t>::min();
        for (size_t i = 0; i < h.periods.size(); ++i) {
            const auto &p = h.periods[i];
            if (!legal_utc(p.start) || (p.end && (!legal_utc(p.end) || p.end < p.start)) ||
                p.start < previous_end || (!p.end && i + 1 < h.periods.size()))
                return fail("Los periodos no pueden solaparse ni terminar antes del inicio");
            previous_end = p.end;
        }
        std::set<CivilDay> notice_days;
        for (const auto &n : h.notices)
            if (!legal_day(n.day) || !notice_days.insert(n.day).second || n.snoozed_until < 0 ||
                (n.snoozed_until && !legal_utc(n.snoozed_until)))
                return fail("Estado de aviso inválido");
    }
    error.clear();
    return true;
}
bool apply_habit_command(HabitDocument &doc, const HabitCommand &cmd, HabitId &result, std::string &error) {
    result = cmd.id;
    auto fail = [&](const char *e) {
        error = e;
        return false;
    };
    if (cmd.operation == HabitOperation::Volume) {
        if (cmd.value < 0 || cmd.value > 100)
            return fail("Volumen inválido");
        doc.volume = cmd.value;
        return true;
    }
    const auto c = cmd.context;
    if (cmd.operation != HabitOperation::Delete && (!c.valid || !legal_utc(c.utc)))
        return fail("Revisar fecha y hora");
    const auto today = civil_day(c.utc, c.offset_minutes);
    auto it = std::find_if(doc.habits.begin(), doc.habits.end(), [&](const Habit &h) { return h.id == cmd.id; });
    if (cmd.operation == HabitOperation::Announce) {
        const auto eligible = pending_habits(doc, c, true);
        for (auto id : cmd.ids) {
            if (std::find(eligible.begin(), eligible.end(), id) == eligible.end())
                continue;
            auto hit =
                std::find_if(doc.habits.begin(), doc.habits.end(), [id](const Habit &h) { return h.id == id; });
            notice_for(*hit, today)->announced = true;
        }
        return validate_habits(doc, error);
    }
    if (cmd.operation == HabitOperation::Save) {
        Habit next = cmd.habit;
        if (next.start_day > today)
            return fail("La fecha de inicio no puede estar en el futuro");
        if (it != doc.habits.end()) {
            if (next.kind != it->kind && (!it->done.empty() || !it->periods.empty()))
                return fail("El tipo no puede cambiar después de registrar actividad");
            next.id = it->id;
            next.done = it->done;
            next.periods = it->periods;
            next.notices = it->notices;
            if (next.kind == it->kind)
                next.goals = it->goals;
            if (next.kind == HabitKind::Daily && next.goals.empty())
                next.goals = {{week_start(next.start_day), 5}};
            if (next.kind == HabitKind::Daily && cmd.value >= 1 && cmd.value <= 7) {
                if (cmd.day != week_start(today) && cmd.day != week_start(today) + 7)
                    return fail("La meta se aplica esta semana o la próxima");
                auto goal = std::lower_bound(next.goals.begin(), next.goals.end(), cmd.day,
                                             [](HabitGoal a, CivilDay b) { return a.week < b; });
                if (goal != next.goals.end() && goal->week == cmd.day)
                    goal->days = cmd.value;
                else
                    next.goals.insert(goal, {cmd.day, static_cast<uint8_t>(cmd.value)});
            }
            if (next.kind == HabitKind::Continuous) {
                next.goals.clear();
                next.notice_enabled = false;
                if (next.periods.empty())
                    next.periods.push_back({cmd.start, 0});
            }
            *it = next;
        } else {
            if (cmd.id || doc.next_id == std::numeric_limits<HabitId>::max())
                return fail("No se pudo asignar un identificador");
            next.id = doc.next_id++;
            next.done.clear();
            next.periods.clear();
            next.notices.clear();
            if (next.kind == HabitKind::Daily) {
                if (cmd.value < 1 || cmd.value > 7)
                    return fail("La meta debe ser de 1 a 7 días");
                next.goals = {{week_start(next.start_day), static_cast<uint8_t>(cmd.value)}};
            } else {
                next.goals.clear();
                next.notice_enabled = false;
                next.periods = {{cmd.start, 0}};
            }
            doc.habits.push_back(next);
        }
        result = next.id;
    } else {
        if (it == doc.habits.end())
            return fail("El hábito ya no existe");
        auto &h = *it;
        switch (cmd.operation) {
        case HabitOperation::Delete:
            doc.habits.erase(it);
            break;
        case HabitOperation::ToggleDay: {
            if (h.kind != HabitKind::Daily || cmd.day < h.start_day || cmd.day > today)
                return fail("Solo se pueden corregir días entre el inicio y hoy");
            auto d = std::lower_bound(h.done.begin(), h.done.end(), cmd.day);
            if (d != h.done.end() && *d == cmd.day)
                h.done.erase(d);
            else
                h.done.insert(d, cmd.day);
            break;
        }
        case HabitOperation::Goal: {
            if (h.kind != HabitKind::Daily || cmd.value < 1 || cmd.value > 7 ||
                (cmd.day != week_start(today) && cmd.day != week_start(today) + 7))
                return fail("Elige esta semana o la próxima y una meta de 1-7");
            auto g = std::lower_bound(h.goals.begin(), h.goals.end(), cmd.day,
                                      [](HabitGoal a, CivilDay b) { return a.week < b; });
            if (g != h.goals.end() && g->week == cmd.day)
                g->days = cmd.value;
            else
                h.goals.insert(g, {cmd.day, static_cast<uint8_t>(cmd.value)});
            break;
        }
        case HabitOperation::Interrupt:
            if (h.kind != HabitKind::Continuous || h.periods.empty() || h.periods.back().end)
                return fail("El hábito no está en curso");
            h.periods.back().end = cmd.end;
            if (!cmd.end)
                return fail("Falta la fecha de interrupción");
            break;
        case HabitOperation::Resume:
            if (h.kind != HabitKind::Continuous || (!h.periods.empty() && !h.periods.back().end))
                return fail("El hábito ya está en curso");
            h.periods.push_back({cmd.start, 0});
            break;
        case HabitOperation::CorrectPeriod:
            if (h.kind != HabitKind::Continuous || cmd.value < 0 ||
                static_cast<size_t>(cmd.value) >= h.periods.size())
                return fail("Periodo inválido");
            // A correction cannot silently turn a closed period into a resumed one.
            if (bool(h.periods[cmd.value].end) != bool(cmd.end))
                return fail("Conserva el estado abierto o cerrado del periodo");
            h.periods[cmd.value] = {cmd.start, cmd.end};
            break;
        case HabitOperation::Snooze:
        case HabitOperation::Omit: {
            if (h.kind != HabitKind::Daily)
                return fail("Este hábito no admite avisos diarios");
            auto *n = notice_for(h, today);
            if (cmd.operation == HabitOperation::Omit) {
                n->omitted = true;
                n->snoozed_until = 0;
            } else {
                n->announced = false;
                n->snoozed_until = c.utc + 900;
            }
            break;
        }
        default:
            return fail("Acción de hábito no válida");
        }
    }
    for (const auto &h : doc.habits) {
        if (cmd.operation == HabitOperation::Delete || h.id != result)
            continue;
        for (auto d : h.done)
            if (d > today)
                return fail("No se pueden registrar fechas futuras");
        for (auto p : h.periods)
            if (p.start > c.utc || p.end > c.utc)
                return fail("No se permiten fechas futuras");
    }
    return validate_habits(doc, error);
}
std::vector<HabitId> pending_habits(const HabitDocument &doc, HabitContext c, bool only_unannounced) {
    std::vector<HabitId> result;
    if (!c.valid || !legal_utc(c.utc))
        return result;
    const auto today = civil_day(c.utc, c.offset_minutes);
    const int64_t midnight = today * 86400LL - c.offset_minutes * 60LL;
    for (const auto &h : doc.habits) {
        if (h.kind != HabitKind::Daily || !h.notice_enabled || !habit_clock_valid(h, c) || has_done(h, today) ||
            weekly_done(h, today) >= weekly_goal(h, today))
            continue;
        const auto *n = notice_for(h, today);
        if (n && (n->omitted || (only_unannounced && n->announced)))
            continue;
        const int64_t due = n && n->snoozed_until ? n->snoozed_until : midnight + h.notice_minute * 60;
        if (c.utc >= due)
            result.push_back(h.id);
    }
    return result;
}
HabitNoticeMode notice_mode_for(const HabitDocument &doc, const std::vector<HabitId> &ids) {
    auto mode = HabitNoticeMode::Visual;
    for (auto id : ids)
        if (const auto *h = find_habit(doc, id))
            if (h->notice_mode > mode)
                mode = h->notice_mode;
    return mode;
}
} // namespace el_sistema
