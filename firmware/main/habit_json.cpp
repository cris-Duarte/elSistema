#include "cJSON.h"
#include "habit_repository.hpp"
#include <cmath>
#include <limits>
#include <memory>

namespace el_sistema {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
bool number(const cJSON *o, const char *key, int64_t minimum, int64_t maximum, int64_t &out) {
    const auto *n = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsNumber(n) || !std::isfinite(n->valuedouble) || std::floor(n->valuedouble) != n->valuedouble ||
        n->valuedouble < minimum || n->valuedouble > maximum)
        return false;
    out = static_cast<int64_t>(n->valuedouble);
    return true;
}
bool boolean(const cJSON *o, const char *key, bool &out) {
    const auto *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsBool(v))
        return false;
    out = cJSON_IsTrue(v);
    return true;
}
bool string(const cJSON *o, const char *key, std::string &out) {
    const auto *v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (!cJSON_IsString(v) || !v->valuestring)
        return false;
    out = v->valuestring;
    return true;
}
cJSON *array(const cJSON *o, const char *key) {
    auto *a = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsArray(a) ? a : nullptr;
}
bool add_num(cJSON *o, const char *k, int64_t n) {
    return cJSON_AddNumberToObject(o, k, static_cast<double>(n));
}
bool add_bool(cJSON *o, const char *k, bool b) {
    return cJSON_AddBoolToObject(o, k, b);
}
} // namespace
bool habit_json_encode(const HabitDocument &doc, std::string &output, std::string &error) {
    if (!validate_habits(doc, error))
        return false;
    Json root(cJSON_CreateObject(), cJSON_Delete);
    if (!root) {
        error = "Sin memoria para guardar hábitos";
        return false;
    }
    bool ok = add_num(root.get(), "schema_version", doc.schema_version) &&
              add_num(root.get(), "next_id", doc.next_id) && add_num(root.get(), "volume", doc.volume);
    auto *habits = cJSON_AddArrayToObject(root.get(), "habits");
    ok &= habits != nullptr;
    for (const auto &h : doc.habits) {
        auto *o = cJSON_CreateObject();
        if (!o || !habits) {
            if (o)
                cJSON_Delete(o);
            ok = false;
            break;
        }
        cJSON_AddItemToArray(habits, o);
        ok &= add_num(o, "id", h.id) && cJSON_AddStringToObject(o, "name", h.name.c_str()) &&
              cJSON_AddStringToObject(o, "description", h.description.c_str()) &&
              add_num(o, "kind", static_cast<int>(h.kind)) && add_num(o, "start_day", h.start_day) &&
              add_bool(o, "notice_enabled", h.notice_enabled) && add_num(o, "notice_minute", h.notice_minute) &&
              add_num(o, "notice_mode", static_cast<int>(h.notice_mode));
        auto *done = cJSON_AddArrayToObject(o, "done");
        auto *goals = cJSON_AddArrayToObject(o, "goals");
        auto *periods = cJSON_AddArrayToObject(o, "periods");
        auto *notices = cJSON_AddArrayToObject(o, "notices");
        if (!done || !goals || !periods || !notices) {
            ok = false;
            break;
        }
        for (auto d : h.done) {
            auto *n = cJSON_CreateNumber(d);
            if (!n) {
                ok = false;
                break;
            }
            cJSON_AddItemToArray(done, n);
        }
        for (auto g : h.goals) {
            auto *n = cJSON_CreateObject();
            if (!n) {
                ok = false;
                break;
            }
            cJSON_AddItemToArray(goals, n);
            ok &= add_num(n, "week", g.week) && add_num(n, "days", g.days);
        }
        for (auto p : h.periods) {
            auto *n = cJSON_CreateObject();
            if (!n) {
                ok = false;
                break;
            }
            cJSON_AddItemToArray(periods, n);
            ok &= add_num(n, "start", p.start) && add_num(n, "end", p.end);
        }
        for (auto a : h.notices) {
            auto *n = cJSON_CreateObject();
            if (!n) {
                ok = false;
                break;
            }
            cJSON_AddItemToArray(notices, n);
            ok &= add_num(n, "day", a.day) && add_bool(n, "announced", a.announced) &&
                  add_bool(n, "omitted", a.omitted) && add_num(n, "snoozed_until", a.snoozed_until);
        }
    }
    char *text = ok ? cJSON_PrintUnformatted(root.get()) : nullptr;
    if (!text) {
        error = "Sin memoria para serializar hábitos";
        return false;
    }
    output = text;
    cJSON_free(text);
    error.clear();
    return true;
}
bool habit_json_decode(const std::string &json, HabitDocument &document, std::string &error) {
    const char *end = nullptr;
    Json root(cJSON_ParseWithLengthOpts(json.c_str(), json.size() + 1, &end, true), cJSON_Delete);
    auto fail = [&]() {
        error = "Documento de hábitos incompleto, corrupto o incompatible";
        return false;
    };
    if (!root || !cJSON_IsObject(root.get()))
        return fail();
    int64_t n = 0;
    HabitDocument next;
    if (!number(root.get(), "schema_version", 1, 1, n))
        return fail();
    next.schema_version = n;
    if (!number(root.get(), "next_id", 1, UINT32_MAX, n))
        return fail();
    next.next_id = n;
    if (!number(root.get(), "volume", 0, 100, n))
        return fail();
    next.volume = n;
    const auto *habits = array(root.get(), "habits");
    if (!habits)
        return fail();
    const cJSON *o;
    cJSON_ArrayForEach(o, habits) {
        Habit h;
        if (!cJSON_IsObject(o) || !number(o, "id", 1, UINT32_MAX, n))
            return fail();
        h.id = n;
        if (!string(o, "name", h.name) || !string(o, "description", h.description) || !number(o, "kind", 0, 1, n))
            return fail();
        h.kind = static_cast<HabitKind>(n);
        if (!number(o, "start_day", INT32_MIN, INT32_MAX, n))
            return fail();
        h.start_day = n;
        if (!boolean(o, "notice_enabled", h.notice_enabled) || !number(o, "notice_minute", 0, 1439, n))
            return fail();
        h.notice_minute = n;
        if (!number(o, "notice_mode", 0, 2, n))
            return fail();
        h.notice_mode = static_cast<HabitNoticeMode>(n);
        auto *done = array(o, "done");
        auto *goals = array(o, "goals");
        auto *periods = array(o, "periods");
        auto *notices = array(o, "notices");
        if (!done || !goals || !periods || !notices)
            return fail();
        const cJSON *v;
        cJSON_ArrayForEach(v, done) {
            if (!cJSON_IsNumber(v) || !std::isfinite(v->valuedouble) || v->valuedouble < INT32_MIN ||
                v->valuedouble > INT32_MAX || std::floor(v->valuedouble) != v->valuedouble)
                return fail();
            h.done.push_back(static_cast<CivilDay>(v->valuedouble));
        }
        cJSON_ArrayForEach(v, goals) {
            HabitGoal g;
            if (!number(v, "week", INT32_MIN, INT32_MAX, n))
                return fail();
            g.week = n;
            if (!number(v, "days", 1, 7, n))
                return fail();
            g.days = n;
            h.goals.push_back(g);
        }
        cJSON_ArrayForEach(v, periods) {
            HabitPeriod p;
            if (!number(v, "start", -62135596800LL, 253402300799LL, p.start) ||
                !number(v, "end", -62135596800LL, 253402300799LL, p.end))
                return fail();
            h.periods.push_back(p);
        }
        cJSON_ArrayForEach(v, notices) {
            HabitNoticeDay a;
            if (!number(v, "day", INT32_MIN, INT32_MAX, n))
                return fail();
            a.day = n;
            if (!boolean(v, "announced", a.announced) || !boolean(v, "omitted", a.omitted) ||
                !number(v, "snoozed_until", 0, 253402300799LL, a.snoozed_until))
                return fail();
            h.notices.push_back(a);
        }
        next.habits.push_back(std::move(h));
    }
    if (!validate_habits(next, error))
        return false;
    document = std::move(next);
    error.clear();
    return true;
}
} // namespace el_sistema
