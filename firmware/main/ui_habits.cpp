#include "esp_timer.h"
#include "fonts/fonts.h"
#include "ui.hpp"
#include <algorithm>
#include <cstdio>

namespace el_sistema {
namespace {
constexpr uint32_t Paper = theme::HabitPaper, Sheet = theme::HabitSheet, Ink = theme::HabitInk,
                   Green = theme::HabitGreen, Blue = theme::HabitBlue, Muted = theme::HabitMuted;
void position(lv_obj_t *o, int x, int y, int w, int h) {
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
}
void text_at(lv_obj_t *o, int x, int y, int width) {
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, width);
}
const char *mode_text(HabitNoticeMode mode) {
    switch (mode) {
    case HabitNoticeMode::Visual:
        return "Visual";
    case HabitNoticeMode::Tone:
        return "Tono";
    case HabitNoticeMode::Voice:
        return "Voz";
    }
    return "Visual";
}
std::string hhmm(int minute) {
    char s[8];
    std::snprintf(s, sizeof(s), "%02d:%02d", minute / 60, minute % 60);
    return s;
}
bool parse_hhmm(const std::string &s, int &minute) {
    if (s.size() != 5 || s[2] != ':')
        return false;
    for (size_t i : {0U, 1U, 3U, 4U})
        if (s[i] < '0' || s[i] > '9')
            return false;
    int h = (s[0] - '0') * 10 + s[1] - '0', m = (s[3] - '0') * 10 + s[4] - '0';
    minute = h * 60 + m;
    return h < 24 && m < 60;
}
} // namespace
bool AppUi::habit_view() const {
    return view_ >= View::Habits;
}
bool AppUi::habit_critical() const {
    return view_ == View::HabitEditor || view_ == View::HabitPeriod;
}
HabitContext AppUi::habit_context() const {
    return {clock_.now_utc(), state_.settings.utc_offset_minutes, clock_.is_valid()};
}
lv_obj_t *AppUi::habit_panel(lv_obj_t *parent, int x, int y, int w, int h, bool scroll) {
    auto *o = panel(parent, x, y, w, h, scroll);
    lv_obj_set_style_bg_color(o, lv_color_hex(Sheet), 0);
    lv_obj_set_style_border_color(o, lv_color_hex(Ink), 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_radius(o, 3, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}
lv_obj_t *AppUi::habit_button(lv_obj_t *parent, const char *text, int x, int y, int w, ActionCode code, HabitId id,
                              int value, uint32_t bg) {
    auto *o = button(parent, text, x, y, w, 48, code, id, value, nullptr, bg, Ink);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_radius(o, 3, 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg == Green || bg == Blue ? Blue : Paper), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(o, lv_color_hex(Blue), LV_STATE_PRESSED);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(o, lv_color_hex(Paper), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(o, lv_color_hex(bg == Green || bg == Blue ? 0xFFFFFF : Ink), 0);
    lv_obj_set_style_text_color(o, lv_color_hex(Muted), LV_STATE_DISABLED);
    if (auto *caption = lv_obj_get_child(o, 0)) {
        // Inherit the button state color; LVGL 9.2 has no STATE_TRICKLE flag.
        lv_obj_remove_local_style_prop(caption, LV_STYLE_TEXT_COLOR, 0);
    }
    return o;
}
lv_obj_t *AppUi::habit_field(lv_obj_t *parent, int index, const char *caption, int y, int w, bool multiline) {
    text_at(label(parent, caption, &font_rajdhani_20, Ink), 16, y, w - 32);
    auto *field = lv_textarea_create(parent);
    position(field, 16, y + 28, w - 32, multiline ? 106 : 56);
    lv_textarea_set_one_line(field, !multiline);
    lv_textarea_set_text(field, habit_fields_[index].c_str());
    lv_textarea_set_max_length(field, index == 0 ? 81 : index == 1 ? 241 : 24);
    lv_obj_set_style_bg_color(field, lv_color_hex(Sheet), 0);
    lv_obj_set_style_text_color(field, lv_color_hex(Ink), 0);
    lv_obj_set_style_text_font(field, &font_poppins_16, 0);
    lv_obj_set_style_border_width(field, 2, 0);
    lv_obj_set_style_border_color(field, lv_color_hex(index == habit_focus_index_ ? Blue : Ink), 0);
    lv_obj_set_style_radius(field, 2, 0);
    lv_obj_add_event_cb(field, action_callback, LV_EVENT_FOCUSED, make_action(ActionCode::HabitField, 0, index));
    lv_obj_add_event_cb(field, action_callback, LV_EVENT_CLICKED, make_action(ActionCode::HabitField, 0, index));
    habit_textareas_[index] = field;
    return field;
}
void AppUi::render_habits() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    const bool portrait = is_portrait();
    text_at(label(screen_, portrait ? "HÁBITOS" : "HÁBITOS · HOY Y ESTA SEMANA", &font_rajdhani_40, Ink), 20, 56,
            w - 290);
    if (portrait)
        text_at(label(screen_, "HOY Y ESTA SEMANA", &font_rajdhani_20, Muted), 20, 102, w - 40);
    const int filter_y = portrait ? 138 : 116;
    habit_button(screen_, "INICIO", w - 144, 60, 124, ActionCode::GoHome);
    habit_button(screen_, "SONIDO", w - 280, 60, 124, ActionCode::HabitSettings);
    const bool detail = portrait && view_ == View::HabitDetail;
    if (detail) {
        habit_button(screen_, "VOLVER", 20, filter_y, 124, ActionCode::HabitBack);
        habit_button(screen_, "EDITAR", 156, filter_y, 124, ActionCode::HabitEdit, habit_selected_);
        habit_button(screen_, "ELIMINAR", 292, filter_y, 148, ActionCode::HabitAskDelete, habit_selected_);
    } else {
        for (int i = 0; i < 3; ++i)
            habit_button(screen_,
                         i == 0   ? "TODOS"
                         : i == 1 ? "DIARIOS"
                                  : "CONTINUOS",
                         20 + i * 140, filter_y, 128, ActionCode::HabitFilter, 0, i,
                         habit_filter_ == i ? Blue : Sheet);
        habit_button(screen_, "+ CREAR", w - 160, filter_y, 140, ActionCode::HabitNew, 0, 0, Blue);
    }
    std::string message = habit_message_.empty() ? habits_snapshot_.error : habit_message_;
    if (message.empty() && habits_snapshot_.storage_warning)
        message = "Almacenamiento al 80 % o más. No se borrará el historial.";
    if (!clock_.is_valid())
        message = "Revisar fecha y hora · No se calculan duraciones ni avisos";
    habit_error_label_ = label(screen_, message.c_str(), &font_poppins_16, Ink);
    text_at(habit_error_label_, 20, filter_y + 58, w - 40);
    lv_obj_update_layout(habit_error_label_);
    const int top = filter_y + 70 + (message.empty() ? 0 : lv_obj_get_height(habit_error_label_) + 12);
    if (detail) {
        auto *area = habit_panel(screen_, 16, top, w - 32, h - top - 16, true);
        habit_detail_ = area;
        render_habit_detail(area, w - 36);
        lv_obj_update_layout(area);
        lv_obj_scroll_to_y(area, habit_detail_scroll_, LV_ANIM_OFF);
        return;
    }
    const int list_w = portrait ? w - 32 : std::min(480, w - 720);
    auto *list = habit_panel(screen_, 16, top, list_w, h - top - 16, true);
    habit_list_ = list;
    int y = 16;
    bool any = false;
    for (const auto &habit : habits_snapshot_.document.habits) {
        if (habit_filter_ == 1 && habit.kind != HabitKind::Daily)
            continue;
        if (habit_filter_ == 2 && habit.kind != HabitKind::Continuous)
            continue;
        any = true;
        auto *card = habit_button(list, habit.name.c_str(), 12, y, list_w - 28, ActionCode::HabitSelect, habit.id,
                                  0, habit.id == habit_selected_ ? Paper : Sheet);
        lv_obj_set_height(card, habit.kind == HabitKind::Daily ? 176 : 136);
        auto *title = lv_obj_get_child(card, 0);
        text_at(title, 12, 4, list_w - 64);
        lv_obj_set_style_text_font(title, &font_rajdhani_28, 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
        const auto context = habit_context();
        const auto today = civil_day(context.utc, context.offset_minutes);
        std::string summary;
        if (!habit_clock_valid(habit, context))
            summary = "Revisar fecha y hora";
        else if (habit.kind == HabitKind::Daily)
            summary = std::to_string(weekly_done(habit, today)) + "/" + std::to_string(weekly_goal(habit, today)) +
                      " días · " + (has_done(habit, today) ? "Hoy: cumplido" : "Hoy: sin registro");
        else
            summary = habit.periods.empty()
                          ? "Sin periodos"
                          : std::string(habit.periods.back().end ? "Detenido · " : "En curso · ") +
                                habit_duration(period_seconds(habit.periods.back(), context));
        auto *value =
            label(card, summary.c_str(), &font_rajdhani_20, habit.kind == HabitKind::Daily ? Green : Blue);
        text_at(value, 12, 42, list_w - 64);
        lv_obj_remove_flag(title, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(value, LV_OBJ_FLAG_CLICKABLE);
        if (habit.kind == HabitKind::Continuous)
            habit_counters_.push_back({habit.id, value});
        if (habit.kind == HabitKind::Daily) {
            static const char *days[] = {"L", "M", "X", "J", "V", "S", "D"};
            for (int i = 0; i < 7; ++i) {
                const CivilDay day = week_start(today) + i;
                const bool done = has_done(habit, day);
                const std::string caption = std::string(days[i]) + (done ? " X" : "");
                auto *cell = habit_button(card, caption.c_str(), 12 + i * 50, 108, 48, ActionCode::HabitDay,
                                          habit.id, day, done ? Green : Sheet);
                if (!context.valid || day < habit.start_day || day > today)
                    lv_obj_add_state(cell, LV_STATE_DISABLED);
                if (day == today)
                    lv_obj_set_style_border_color(cell, lv_color_hex(Blue), 0);
            }
        }
        y += habit.kind == HabitKind::Daily ? 188 : 148;
    }
    if (!any) {
        auto *empty = label(list,
                            habits_snapshot_.available
                                ? "A tu ritmo\n\nDiarios: marca los días cumplidos y elige una meta semanal "
                                  "flexible.\n\nContinuos: mide el tiempo desde un comienzo, con historial de "
                                  "interrupciones.\n\nCrea tu primer hábito. Sin ejemplos automáticos, sin XP."
                                : "Preparando el almacenamiento de hábitos...",
                            &font_rajdhani_28, Ink);
        text_at(empty, 20, 20, list_w - 44);
    }
    if (!portrait) {
        const int detail_x = list_w + 28;
        auto *area = habit_panel(screen_, detail_x, top, w - detail_x - 16, h - top - 16, true);
        habit_detail_ = area;
        render_habit_detail(area, w - detail_x - 20);
        lv_obj_update_layout(area);
        lv_obj_scroll_to_y(area, habit_detail_scroll_, LV_ANIM_OFF);
    }
    lv_obj_update_layout(list);
    lv_obj_scroll_to_y(list, habit_list_scroll_, LV_ANIM_OFF);
}
void AppUi::render_habit_detail(lv_obj_t *parent, int width) {
    const auto *habit = find_habit(habits_snapshot_.document, habit_selected_);
    if (!habit) {
        text_at(label(parent, "Selecciona un hábito para ver su evolución.", &font_rajdhani_28, Ink), 24, 24,
                width - 48);
        return;
    }
    const auto c = habit_context();
    const auto today = civil_day(c.utc, c.offset_minutes);
    auto *name = label(parent, habit->name.c_str(), &font_rajdhani_40, Ink);
    text_at(name, 24, 16, width - 48);
    lv_obj_update_layout(name);
    int y = lv_obj_get_height(name) + 28;
    if (!habit->description.empty()) {
        auto *description = label(parent, habit->description.c_str(), &font_poppins_16, Muted);
        text_at(description, 24, y, width - 48);
        lv_obj_update_layout(description);
        y += lv_obj_get_height(description) + 16;
    }
    if (!is_portrait()) {
        habit_button(parent, "EDITAR", 24, y, 120, ActionCode::HabitEdit, habit->id);
        habit_button(parent, "ELIMINAR", 156, y, 140, ActionCode::HabitAskDelete, habit->id);
        y += 64;
    }
    if (!habit_clock_valid(*habit, c)) {
        text_at(label(parent, "Revisar fecha y hora", &font_rajdhani_28, Blue), 24, y, width - 48);
        y += 48;
        habit_button(parent, "REVISAR RELOJ", 24, y, 220, ActionCode::HabitClock);
        y += 64;
        if (!c.valid && habit->kind == HabitKind::Daily)
            return;
    }
    if (habit->kind == HabitKind::Daily) {
        const auto count = weekly_done(*habit, today);
        const auto goal = weekly_goal(*habit, today);
        const std::string progress = std::to_string(count) + "/" + std::to_string(goal) + " días · " +
                                     (count >= goal ? "Meta alcanzada" : "Esta semana");
        text_at(label(parent, progress.c_str(), &font_rajdhani_40, Green), 24, y, width - 48);
        y += 60;
        static const char *names[] = {"L", "M", "X", "J", "V", "S", "D"};
        for (int i = 0; i < 7; ++i) {
            const CivilDay d = week_start(today) + i;
            const bool enabled = c.valid && d >= habit->start_day && d <= today;
            auto *cell = habit_button(parent, names[i], 24 + i * 58, y, 50, ActionCode::HabitDay, habit->id, d,
                                      has_done(*habit, d) ? Green : Sheet);
            if (d == today) {
                lv_obj_set_style_border_color(cell, lv_color_hex(Blue), 0);
                lv_obj_set_style_border_width(cell, 3, 0);
            }
            if (!enabled)
                lv_obj_add_state(cell, LV_STATE_DISABLED);
        }
        y += 60;
        habit_button(parent, has_done(*habit, today) ? "DESMARCAR HOY" : "HECHO HOY", 24, y, 220,
                     ActionCode::HabitToday, habit->id, 0, Green);
        y += 64;
        const CivilDay first = week_start(today) - 77 - habit_mosaic_offset_ * 84, last = first + 83;
        const std::string range = day_text(first) + " a " + day_text(last);
        text_at(label(parent, range.c_str(), &font_rajdhani_28, Ink), 24, y, width - 48);
        y += 40;
        habit_button(parent, "ANTERIORES", 24, y, 156, ActionCode::HabitMosaic, 0, 1);
        auto *next = habit_button(parent, "RECIENTES", 192, y, 156, ActionCode::HabitMosaic, 0, -1);
        if (habit_mosaic_offset_ == 0)
            lv_obj_add_state(next, LV_STATE_DISABLED);
        y += 64;
        habit_calendar_ = std::make_unique<HabitCalendar>();
        const HabitId id = habit->id;
        habit_calendar_->create(parent, 0, y, *habit, first, today, c.valid, [this, id](CivilDay day) {
            const Action action{this, ActionCode::HabitDay, id, day};
            dispatch_habit(action);
        });
        y += HabitCalendar::Height + 12;
        text_at(label(parent, "X: cumplido · vacío: sin registro · azul: hoy · -: fuera del periodo",
                      &font_poppins_16, Ink),
                24, y, width - 48);
        y += 60;
        if (habit->start_day != week_start(habit->start_day)) {
            text_at(
                label(parent, "Semana inicial parcial: se conserva la meta completa.", &font_poppins_16, Muted),
                24, y, width - 48);
            y += 56;
        }
        const std::string notice =
            habit->notice_enabled ? "Aviso: " + hhmm(habit->notice_minute) + " · " + mode_text(habit->notice_mode)
                                  : "Avisos desactivados";
        text_at(label(parent, notice.c_str(), &font_rajdhani_28, Blue), 24, y, width - 48);
        y += 48;
    } else {
        const bool running = !habit->periods.empty() && !habit->periods.back().end;
        text_at(label(parent, running ? "EN CURSO" : "DETENIDO · ÚLTIMO PERIODO", &font_rajdhani_28, Blue), 24, y,
                width - 48);
        y += 44;
        const auto elapsed = habit_clock_valid(*habit, c) && !habit->periods.empty()
                                 ? period_seconds(habit->periods.back(), c)
                                 : -1;
        auto *counter = label(parent, habit_duration(elapsed).c_str(), &font_rajdhani_64, Ink);
        text_at(counter, 24, y, width - 48);
        habit_counters_.push_back({habit->id, counter});
        lv_obj_update_layout(counter);
        y += lv_obj_get_height(counter) + 16;
        habit_button(parent, running ? "REGISTRAR INTERRUPCIÓN" : "RETOMAR", 24, y, 300,
                     ActionCode::HabitPeriodOpen, habit->id, running ? 1 : 0, Blue);
        y += 64;
        habit_best_ = label(parent, ("Mejor periodo: " + habit_duration(best_period_seconds(*habit, c))).c_str(),
                            &font_rajdhani_28, Blue);
        text_at(habit_best_, 24, y, width - 48);
        lv_obj_update_layout(habit_best_);
        y += lv_obj_get_height(habit_best_) + 16;
        text_at(label(parent, "HISTORIAL · TOCA PARA CORREGIR", &font_rajdhani_28, Ink), 24, y, width - 48);
        y += 44;
        for (size_t reverse = habit->periods.size(); reverse > 0; --reverse) {
            const size_t index = reverse - 1;
            const auto &p = habit->periods[index];
            const std::string row = datetime_text(p.start, c.offset_minutes) + "\n" +
                                    (p.end ? datetime_text(p.end, c.offset_minutes) : "En curso") + "\n" +
                                    habit_duration(period_seconds(p, c));
            auto *b = habit_button(parent, row.c_str(), 24, y, width - 48, ActionCode::HabitPeriodOpen, habit->id,
                                   static_cast<int>(index) + 2);
            lv_obj_set_height(b, 150);
            auto *caption = lv_obj_get_child(b, 0);
            lv_obj_set_style_text_font(caption, &font_rajdhani_20, 0);
            lv_obj_set_width(caption, width - 80);
            lv_obj_center(caption);
            y += 162;
        }
    }
    // Bottom spacer participates in scroll extents; no overlay covers the last row.
    auto *spacer = label(parent, " ", &font_poppins_16, Ink);
    lv_obj_set_pos(spacer, 24, y + 16);
}
void AppUi::capture_habit_fields() {
    if (habit_form_)
        habit_scroll_ = lv_obj_get_scroll_y(habit_form_);
    for (size_t i = 0; i < habit_textareas_.size(); ++i)
        if (habit_textareas_[i])
            habit_fields_[i] = lv_textarea_get_text(habit_textareas_[i]);
}
void AppUi::open_habit_editor(HabitId id) {
    habit_editing_ = id;
    habit_draft_ = Habit{};
    habit_message_.clear();
    habit_goal_next_ = false;
    habit_goal_ = 5;
    habit_scroll_ = 0;
    const auto c = habit_context();
    habit_draft_.start_day = civil_day(c.utc, c.offset_minutes);
    habit_fields_.fill("");
    habit_fields_[2] = day_text(habit_draft_.start_day);
    habit_fields_[3] = "18:00";
    habit_fields_[4] = datetime_text(c.utc, c.offset_minutes);
    if (const auto *h = find_habit(habits_snapshot_.document, id)) {
        habit_draft_ = *h;
        habit_fields_[0] = h->name;
        habit_fields_[1] = h->description;
        habit_fields_[2] = day_text(h->start_day);
        habit_fields_[3] = hhmm(h->notice_minute);
        habit_goal_ = weekly_goal(*h, civil_day(c.utc, c.offset_minutes));
        if (!h->periods.empty())
            habit_fields_[4] = datetime_text(h->periods.front().start, c.offset_minutes);
    }
    keyboard_upper_ = false;
    keyboard_page_ = KeyboardPage::Letters;
    habit_focus_index_ = 0;
    view_ = View::HabitEditor;
}
void AppUi::render_habit_editor() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    const int keyboard_h = is_portrait() ? 404 : 292, keyboard_y = h - keyboard_h;
    text_at(label(screen_, habit_editing_ ? "EDITAR HÁBITO" : "NUEVO HÁBITO", &font_rajdhani_40, Ink), 20, 56,
            w - 40);
    const int action_y = keyboard_y - 60;
    auto *form = habit_panel(screen_, 16, 110, w - 32, action_y - 122, true);
    habit_form_ = form;
    habit_field(form, 0, "NOMBRE · HASTA 80 CARACTERES", 12, w - 36);
    int y = 108;
    habit_field(form, 1, "DESCRIPCIÓN · OPCIONAL, HASTA 240", y, w - 36, true);
    y += 152;
    const bool fixed = !habit_draft_.done.empty() || !habit_draft_.periods.empty();
    text_at(label(form, fixed ? "TIPO · FIJO PORQUE YA EXISTEN REGISTROS" : "TIPO", &font_rajdhani_20, Ink), 16, y,
            w - 64);
    y += 30;
    auto *daily = habit_button(form, "DIARIO", 16, y, 180, ActionCode::HabitKind, 0, 0,
                               habit_draft_.kind == HabitKind::Daily ? Green : Sheet);
    auto *continuous = habit_button(form, "CONTINUO", 208, y, 180, ActionCode::HabitKind, 0, 1,
                                    habit_draft_.kind == HabitKind::Continuous ? Blue : Sheet);
    if (fixed) {
        lv_obj_add_state(daily, LV_STATE_DISABLED);
        lv_obj_add_state(continuous, LV_STATE_DISABLED);
    }
    y += 64;
    if (habit_draft_.kind == HabitKind::Daily) {
        habit_field(form, 2, "FECHA INICIAL · AAAA-MM-DD (PUEDE SER PASADA)", y, w - 36);
        y += 100;
        text_at(label(form, ("META FLEXIBLE · " + std::to_string(habit_goal_) + " DÍAS POR SEMANA").c_str(),
                      &font_rajdhani_28, Ink),
                16, y, w - 64);
        y += 40;
        habit_button(form, "-", 16, y, 60, ActionCode::HabitGoalAdjust, 0, -1);
        habit_button(form, "+", 88, y, 60, ActionCode::HabitGoalAdjust, 0, 1);
        if (habit_editing_)
            habit_button(form, habit_goal_next_ ? "LA PRÓXIMA SEMANA" : "ESTA SEMANA", 164, y, 270,
                         ActionCode::HabitGoalWhen);
        y += 64;
        habit_button(form, habit_draft_.notice_enabled ? "AVISO: ACTIVADO" : "AVISO: DESACTIVADO", 16, y, 280,
                     ActionCode::HabitNoticeToggle, 0, 0, habit_draft_.notice_enabled ? Blue : Sheet);
        y += 64;
        if (habit_draft_.notice_enabled) {
            habit_field(form, 3, "HORA DEL AVISO · HH:MM", y, w - 36);
            y += 100;
            habit_button(form, mode_text(habit_draft_.notice_mode), 16, y, 180, ActionCode::HabitNoticeMode);
            y += 64;
        }
    } else if (!habit_editing_ || habit_draft_.periods.empty()) {
        habit_field(form, 4, "COMIENZO LOCAL · AAAA-MM-DD HH:MM:SS", y, w - 36);
        y += 100;
    } else {
        text_at(label(form, "Corrige fechas desde el historial de periodos.", &font_rajdhani_28, Ink), 16, y,
                w - 64);
        y += 70;
    }
    const std::string message = habit_waiting_ ? "Guardando..."
                                : habits_snapshot_.read_only ? habits_snapshot_.error
                                                            : habit_message_;
    text_at(label(form, message.c_str(), &font_rajdhani_28, Blue), 16, y, w - 64);
    lv_obj_update_layout(form);
    lv_obj_scroll_to_y(form, habit_scroll_, LV_ANIM_OFF);
    habit_button(screen_, "CANCELAR", 20, action_y, 160, ActionCode::HabitBack);
    auto *save = habit_button(screen_, habit_waiting_ ? "GUARDANDO..." : "GUARDAR", w - 200, action_y, 180,
                              ActionCode::HabitSave, 0, 0, Blue);
    if (habit_waiting_ || habits_snapshot_.read_only)
        lv_obj_add_state(save, LV_STATE_DISABLED);
    spanish_keyboard(0, keyboard_y, w, keyboard_h);
    if (habit_textareas_[habit_focus_index_])
        set_active_textarea(habit_textareas_[habit_focus_index_]);
    else {
        habit_focus_index_ = 0;
        set_active_textarea(habit_textareas_[0]);
    }
}
void AppUi::render_habit_period() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    const int keyboard_h = is_portrait() ? 404 : 292, ky = h - keyboard_h, ay = ky - 60;
    const char *title = habit_period_operation_ == HabitOperation::Interrupt ? "REGISTRAR INTERRUPCIÓN"
                        : habit_period_operation_ == HabitOperation::Resume  ? "RETOMAR HÁBITO"
                                                                             : "CORREGIR PERIODO";
    text_at(label(screen_, title, &font_rajdhani_40, Ink), 20, 56, w - 40);
    auto *form = habit_panel(screen_, 16, 110, w - 32, ay - 122, true);
    int y = 12;
    habit_form_ = form;
    if (habit_period_operation_ != HabitOperation::Interrupt) {
        habit_field(form, 4, "COMIENZO LOCAL · AAAA-MM-DD HH:MM:SS", y, w - 36);
        y += 102;
    }
    const auto *habit = find_habit(habits_snapshot_.document, habit_selected_);
    const bool closed =
        habit_period_operation_ == HabitOperation::Interrupt ||
        (habit && habit_period_index_ >= 0 && static_cast<size_t>(habit_period_index_) < habit->periods.size() &&
         habit->periods[habit_period_index_].end);
    if (closed) {
        habit_field(form, 5, "FINAL LOCAL · AAAA-MM-DD HH:MM:SS", y, w - 36);
        y += 102;
    }
    text_at(label(form, "No se permiten fechas futuras ni periodos solapados.", &font_poppins_16, Ink), 16, y,
            w - 64);
    y += 50;
    text_at(label(form, (habit_waiting_ ? "Guardando..." : habit_message_).c_str(), &font_rajdhani_28, Blue), 16,
            y, w - 64);
    lv_obj_update_layout(form);
    lv_obj_scroll_to_y(form, habit_scroll_, LV_ANIM_OFF);
    habit_button(screen_, "CANCELAR", 20, ay, 160, ActionCode::HabitBack);
    auto *save = habit_button(screen_, "APLICAR", w - 200, ay, 180, ActionCode::HabitPeriodSave, 0, 0, Blue);
    if (habit_waiting_)
        lv_obj_add_state(save, LV_STATE_DISABLED);
    spanish_keyboard(0, ky, w, keyboard_h);
    set_active_textarea(habit_textareas_[habit_focus_index_] ? habit_textareas_[habit_focus_index_]
                                                             : habit_textareas_[closed ? 5 : 4]);
}
void AppUi::render_habit_settings() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    text_at(label(screen_, "HÁBITOS · AVISOS", &font_rajdhani_40, Ink), 20, 60, w - 40);
    auto *area = habit_panel(screen_, 16, 122, w - 32, h - 202, true);
    text_at(label(area, ("VOLUMEN COMÚN · " + std::to_string(habits_snapshot_.document.volume) + " %").c_str(),
                  &font_rajdhani_40, Ink),
            24, 24, w - 80);
    habit_button(area, "-10 %", 24, 90, 140, ActionCode::HabitVolume, 0, -10);
    habit_button(area, "+10 %", 180, 90, 140, ActionCode::HabitVolume, 0, 10);
    habit_button(area, "PROBAR TONO", 24, 158, 220, ActionCode::HabitTest, 0, 1);
    habit_button(area, "PROBAR VOZ", 260, 158, 220, ActionCode::HabitTest, 0, 2);
    text_at(
        label(area,
              "Cada hábito diario permite habilitar su aviso, elegir la hora y usar Visual, Tono o Voz.\n\nSin "
              "franja de silencio. Los avisos no se repiten automáticamente y respetan las actividades de audio.",
              &font_rajdhani_28, Ink),
        24, 232, w - 80);
    text_at(label(area, (habit_message_.empty() ? habits_snapshot_.error : habit_message_).c_str(),
                  &font_poppins_16, Blue),
            24, 408, w - 80);
    habit_button(screen_, "VOLVER", 20, h - 64, 160, ActionCode::HabitBack);
}
void AppUi::render_habit_alerts() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    text_at(label(screen_, "HÁBITOS PENDIENTES", &font_rajdhani_40, Ink), 20, 60, w - 210);
    habit_button(screen_, "VOLVER", w - 180, 60, 160, ActionCode::HabitBack);
    auto *list = habit_panel(screen_, 16, 128, w - 32, h - 144, true);
    int y = 16;
    if (habits_snapshot_.pending.empty())
        text_at(label(list, "No hay avisos pendientes hoy.", &font_rajdhani_28, Ink), 20, y, w - 80);
    for (auto id : habits_snapshot_.pending) {
        const auto *habit = find_habit(habits_snapshot_.document, id);
        if (!habit)
            continue;
        auto *name = label(list, habit->name.c_str(), &font_rajdhani_28, Ink);
        text_at(name, 20, y, w - 80);
        lv_obj_update_layout(name);
        y += lv_obj_get_height(name) + 12;
        habit_button(list, "HECHO", 20, y, 164, ActionCode::HabitToday, id, 0, Green);
        habit_button(list, "+15 MIN", 196, y, 164, ActionCode::HabitSnooze, id);
        habit_button(list, "OMITIR HOY", 372, y, 200, ActionCode::HabitOmit, id);
        y += 80;
    }
    text_at(label(list, habit_message_.c_str(), &font_poppins_16, Blue), 20, y, w - 80);
}
void AppUi::render_habit_overlay() {
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    auto *shade = lv_obj_create(screen_);
    position(shade, 0, 0, w, h);
    lv_obj_set_style_bg_color(shade, lv_color_hex(Ink), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_60, 0);
    lv_obj_set_style_border_width(shade, 0, 0);
    lv_obj_set_style_pad_all(shade, 0, 0);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    auto *box = habit_panel(shade, (w - 620) / 2, (h - 300) / 2, 620, 300);
    const bool deletion = overlay_ == Overlay::HabitDelete;
    text_at(label(box, deletion ? "ELIMINAR HÁBITO" : "CONFIRMAR INTERRUPCIÓN", &font_rajdhani_40, Ink), 24, 24,
            572);
    text_at(label(box,
                  deletion ? "También se eliminará todo su historial. Esta acción no se puede deshacer."
                           : "Se cerrará el periodo en la fecha elegida. El hábito quedará detenido hasta que "
                             "decidas retomarlo.",
                  &font_rajdhani_28, Ink),
            24, 86, 572);
    habit_button(box, "CANCELAR", 24, 226, 180, ActionCode::CloseOverlay);
    habit_button(box, "CONFIRMAR", 390, 226, 200,
                 deletion ? ActionCode::HabitDelete : ActionCode::HabitInterruptConfirm, habit_selected_, 0, Blue);
}
bool AppUi::send_habit(HabitCommand command, bool close_editor) {
    if (habit_waiting_)
        return false;
    command.sequence = ++habit_sequence_;
    if (!habits_.post(command)) {
        habit_message_ = "La cola está ocupada o Hábitos no está disponible. Intenta de nuevo.";
        return false;
    }
    habit_waiting_ = command.sequence;
    habit_wait_close_ = close_editor;
    habit_message_ = "Guardando...";
    return true;
}
void AppUi::dispatch_habit(const Action &a) {
    if (a.code == ActionCode::HabitField) {
        habit_focus_index_ = std::clamp(a.value, 0, 5);
        set_active_textarea(habit_textareas_[habit_focus_index_]);
        for (size_t i = 0; i < habit_textareas_.size(); ++i)
            if (habit_textareas_[i])
                lv_obj_set_style_border_color(
                    habit_textareas_[i], lv_color_hex(static_cast<int>(i) == habit_focus_index_ ? Blue : Ink), 0);
        return;
    }
    if (habit_waiting_ && a.code != ActionCode::HabitPending && a.code != ActionCode::HabitSelect)
        return;
    capture_habit_fields();
    const auto before = view_;
    const auto c = habit_context();
    const auto today = civil_day(c.utc, c.offset_minutes);
    HabitCommand command;
    command.id = a.id;
    switch (a.code) {
    case ActionCode::HabitSelect:
        habit_selected_ = a.id;
        habit_detail_scroll_ = 0;
        habit_detail_ = nullptr;
        habit_mosaic_offset_ = 0;
        habit_message_.clear();
        if (is_portrait())
            view_ = View::HabitDetail;
        break;
    case ActionCode::HabitFilter:
        habit_filter_ = a.value;
        view_ = View::Habits;
        break;
    case ActionCode::HabitNew:
        open_habit_editor(0);
        break;
    case ActionCode::HabitEdit:
        open_habit_editor(a.id);
        break;
    case ActionCode::HabitBack:
        if (view_ == View::HabitAlerts)
            view_ = habit_alert_return_;
        else
            view_ =
                habit_selected_ && is_portrait() && view_ != View::HabitDetail ? View::HabitDetail : View::Habits;
        habit_message_.clear();
        break;
    case ActionCode::HabitSave: {
        habit_draft_.name = habit_fields_[0];
        habit_draft_.description = habit_fields_[1];
        command.operation = HabitOperation::Save;
        command.id = habit_editing_;
        command.value = habit_goal_;
        command.day = week_start(today) + (habit_goal_next_ ? 7 : 0);
        if (habit_draft_.kind == HabitKind::Daily) {
            if (!parse_day(habit_fields_[2], habit_draft_.start_day)) {
                habit_message_ = "Fecha inválida. Usa AAAA-MM-DD.";
                break;
            }
            int minute = 0;
            if (habit_draft_.notice_enabled && !parse_hhmm(habit_fields_[3], minute)) {
                habit_message_ = "Hora inválida. Usa HH:MM.";
                break;
            }
            if (habit_draft_.notice_enabled)
                habit_draft_.notice_minute = minute;
        } else if (!habit_editing_ || habit_draft_.periods.empty()) {
            if (!parse_datetime(habit_fields_[4], c.offset_minutes, command.start)) {
                habit_message_ = "Comienzo inválido. Usa AAAA-MM-DD HH:MM:SS.";
                break;
            }
            habit_draft_.start_day = civil_day(command.start, c.offset_minutes);
        }
        command.habit = habit_draft_;
        send_habit(command, true);
        break;
    }
    case ActionCode::HabitToday:
    case ActionCode::HabitDay:
        command.operation = HabitOperation::ToggleDay;
        command.day = a.code == ActionCode::HabitToday ? today : a.value;
        send_habit(command);
        break;
    case ActionCode::HabitMosaic: {
        const auto *h = find_habit(habits_snapshot_.document, habit_selected_);
        const int maximum = h ? std::max<int>(0, (week_start(today) - week_start(h->start_day)) / 84) : 0;
        habit_mosaic_offset_ = std::clamp(habit_mosaic_offset_ + a.value, 0, maximum);
        break;
    }
    case ActionCode::HabitKind:
        if (habit_draft_.done.empty() && habit_draft_.periods.empty())
            habit_draft_.kind = static_cast<HabitKind>(a.value);
        break;
    case ActionCode::HabitGoalAdjust:
        habit_goal_ = std::clamp(habit_goal_ + a.value, 1, 7);
        break;
    case ActionCode::HabitGoalWhen:
        habit_goal_next_ = !habit_goal_next_;
        habit_goal_ = weekly_goal(habit_draft_, today + (habit_goal_next_ ? 7 : 0));
        break;
    case ActionCode::HabitNoticeToggle:
        habit_draft_.notice_enabled = !habit_draft_.notice_enabled;
        break;
    case ActionCode::HabitNoticeMode:
        habit_draft_.notice_mode =
            static_cast<HabitNoticeMode>((static_cast<int>(habit_draft_.notice_mode) + 1) % 3);
        break;
    case ActionCode::HabitAskDelete:
        habit_selected_ = a.id;
        overlay_ = Overlay::HabitDelete;
        break;
    case ActionCode::HabitDelete:
        command.operation = HabitOperation::Delete;
        send_habit(command);
        overlay_ = Overlay::None;
        break;
    case ActionCode::HabitPeriodOpen: {
        habit_selected_ = a.id;
        const auto *habit = find_habit(habits_snapshot_.document, a.id);
        if (!habit)
            break;
        habit_message_.clear();
        habit_period_index_ = a.value - 2;
        habit_scroll_ = 0;
        habit_period_operation_ = a.value == 0   ? HabitOperation::Resume
                                  : a.value == 1 ? HabitOperation::Interrupt
                                                 : HabitOperation::CorrectPeriod;
        habit_fields_[4] = datetime_text(c.utc, c.offset_minutes);
        habit_fields_[5] = habit_fields_[4];
        if (habit_period_index_ >= 0 && static_cast<size_t>(habit_period_index_) < habit->periods.size()) {
            const auto &p = habit->periods[habit_period_index_];
            habit_fields_[4] = datetime_text(p.start, c.offset_minutes);
            habit_fields_[5] = p.end ? datetime_text(p.end, c.offset_minutes) : "";
        }
        habit_focus_index_ = a.value == 1 ? 5 : 4;
        keyboard_page_ = KeyboardPage::Numbers;
        view_ = View::HabitPeriod;
        break;
    }
    case ActionCode::HabitPeriodSave:
    case ActionCode::HabitInterruptConfirm: {
        command.operation = habit_period_operation_;
        command.id = habit_selected_;
        command.value = habit_period_index_;
        if (command.operation != HabitOperation::Interrupt &&
            !parse_datetime(habit_fields_[4], c.offset_minutes, command.start)) {
            habit_message_ = "Comienzo inválido";
            break;
        }
        if (command.operation != HabitOperation::Resume && !habit_fields_[5].empty() &&
            !parse_datetime(habit_fields_[5], c.offset_minutes, command.end)) {
            habit_message_ = "Final inválido";
            break;
        }
        if (command.operation == HabitOperation::Interrupt && a.code != ActionCode::HabitInterruptConfirm) {
            overlay_ = Overlay::HabitInterrupt;
            break;
        }
        send_habit(command, true);
        overlay_ = Overlay::None;
        break;
    }
    case ActionCode::HabitSettings:
        habit_message_.clear();
        view_ = View::HabitSettings;
        break;
    case ActionCode::HabitVolume:
        command.operation = HabitOperation::Volume;
        command.value = std::clamp(habits_snapshot_.document.volume + a.value, 0, 100);
        send_habit(command);
        break;
    case ActionCode::HabitTest:
        command.operation = HabitOperation::TestAudio;
        command.value = a.value;
        send_habit(command);
        break;
    case ActionCode::HabitPending:
        if (view_ != View::HabitAlerts) {
            habit_alert_return_ = view_;
            if (view_ == View::Tetris)
                tetris_.pause();
            view_ = View::HabitAlerts;
        }
        break;
    case ActionCode::HabitSnooze:
    case ActionCode::HabitOmit:
        command.operation = a.code == ActionCode::HabitSnooze ? HabitOperation::Snooze : HabitOperation::Omit;
        send_habit(command);
        break;
    case ActionCode::HabitClock:
        clock_defaults_ready_ = false;
        seed_clock_defaults();
        view_ = View::ClockSetup;
        break;
    default:
        break;
    }
    // A banner action must not rebuild/cancel the game's touch gesture.
    if (habit_view() || view_ != before || a.code == ActionCode::HabitPending)
        schedule_render();
}
void AppUi::update_habits() {
    const auto c = habit_context();
    habits_.clock_context(c);
    HabitSnapshot next;
    if (!habits_.snapshot(next))
        return;
    const bool changed = next.revision != habit_revision_;
    habit_revision_ = next.revision;
    const bool error_changed = next.error != habits_snapshot_.error;
    habits_snapshot_ = std::move(next);
    if ((changed || error_changed) && habit_banner_) {
        lv_obj_delete(habit_banner_);
        habit_banner_ = nullptr;
    }
    if (habit_waiting_ && habits_snapshot_.result_sequence == habit_waiting_) {
        habit_waiting_ = 0;
        if (habits_snapshot_.result_ok) {
            habit_message_.clear();
            if (habit_wait_close_) {
                habit_selected_ = habits_snapshot_.result_id;
                view_ = is_portrait() ? View::HabitDetail : View::Habits;
            }
            if (!find_habit(habits_snapshot_.document, habit_selected_)) {
                habit_selected_ = 0;
                if (view_ == View::HabitDetail)
                    view_ = View::Habits;
            }
        } else {
            habit_message_ = habits_snapshot_.result_error;
            habits_.cancel_audio();
        }
        if (habit_view()) {
            capture_habit_fields();
            schedule_render();
        }
    } else if ((changed || error_changed) && habit_view() && !habit_critical())
        schedule_render();
    if (c.utc != habit_last_second_) {
        habit_last_second_ = c.utc;
        for (auto pair : habit_counters_) {
            const auto *h = find_habit(habits_snapshot_.document, pair.first);
            if (!h || h->periods.empty() || !pair.second)
                continue;
            const int64_t seconds = habit_clock_valid(*h, c) ? period_seconds(h->periods.back(), c) : -1;
            const bool small = lv_obj_get_style_text_font(pair.second, 0) == &font_rajdhani_20;
            const std::string counter =
                (small ? std::string(h->periods.back().end ? "Detenido · " : "En curso · ") : "") +
                habit_duration(seconds);
            lv_label_set_text(pair.second, counter.c_str());
        }
        if (habit_best_)
            if (const auto *h = find_habit(habits_snapshot_.document, habit_selected_))
                lv_label_set_text(habit_best_,
                                  ("Mejor periodo: " + habit_duration(best_period_seconds(*h, c))).c_str());
        const auto today = civil_day(c.utc, c.offset_minutes);
        if (today != habit_last_day_) {
            habit_last_day_ = today;
            if ((view_ == View::Habits || view_ == View::HabitDetail) && !screen_off_)
                schedule_render();
        }
    }
}
void AppUi::render_habit_banner() {
    if (screen_off_ || overlay_ != Overlay::None || habit_critical() || view_ == View::Editor ||
        view_ == View::SearchEditor || view_ == View::DictationSetup || view_ == View::ClockSetup ||
        dictation_.busy() || assistant_voice_.busy())
        return;
    const int w = lv_display_get_horizontal_resolution(display_), h = lv_display_get_vertical_resolution(display_);
    if (!habits_snapshot_.pending.empty()) {
        const std::string caption = "AVISO · " + std::to_string(habits_snapshot_.pending.size());
        if (!habit_pending_indicator_) {
            // Reserve only the unused right end of the status bar; never cover
            // the battery or the game's pause control. Tetris uses its notice slot.
            if (status_power_label_)
                lv_obj_set_width(status_power_label_, 212);
            habit_pending_indicator_ =
                habit_button(screen_, caption.c_str(), view_ == View::Tetris ? 484 : w - 112,
                             view_ == View::Tetris ? 14 : 0, 96, ActionCode::HabitPending, 0, 0, Sheet);
            lv_obj_remove_event_cb(habit_pending_indicator_, action_callback);
            habit_notice_actions_[0] = {this, ActionCode::HabitPending};
            lv_obj_add_event_cb(habit_pending_indicator_, action_callback, LV_EVENT_CLICKED,
                                &habit_notice_actions_[0]);
        } else if (auto *text = lv_obj_get_child(habit_pending_indicator_, 0))
            lv_label_set_text(text, caption.c_str());
    } else if (habit_pending_indicator_) {
        lv_obj_delete(habit_pending_indicator_);
        habit_pending_indicator_ = nullptr;
        if (status_power_label_)
            lv_obj_set_width(status_power_label_, 320);
    }
    const int64_t now = esp_timer_get_time();
    habit_banner_ids_.erase(std::remove_if(habit_banner_ids_.begin(), habit_banner_ids_.end(),
                                           [&](HabitId id) {
                                               return std::find(habits_snapshot_.pending.begin(),
                                                                habits_snapshot_.pending.end(),
                                                                id) == habits_snapshot_.pending.end();
                                           }),
                            habit_banner_ids_.end());
    if (now >= habit_banner_until_ || habit_banner_ids_.empty()) {
        if (habit_banner_) {
            lv_obj_delete(habit_banner_);
            habit_banner_ = nullptr;
        }
        return;
    }
    if (habit_banner_)
        return;
    habit_banner_ = habit_panel(screen_, 16, h - 140, w - 32, 124);
    const auto *habit = find_habit(habits_snapshot_.document, habit_banner_ids_.front());
    const std::string title = !habit_message_.empty()        ? habit_message_
                              : habit_banner_ids_.size() > 1 ? std::to_string(habit_banner_ids_.size()) +
                                                                   " hábitos pendientes · Revisa la lista"
                              : habit ? habit->name
                                      : "Hábito pendiente";
    auto *name = label(habit_banner_, title.c_str(), &font_rajdhani_28, Ink);
    text_at(name, 12, 8, w - 64);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    const int bw = (w - 80) / 4;
    const ActionCode codes[] = {ActionCode::HabitToday, ActionCode::HabitSnooze, ActionCode::HabitOmit,
                                ActionCode::HabitPending};
    const char *titles[] = {"HECHO", "+15 MIN", "OMITIR HOY", "VER LISTA"};
    for (int i = 0; i < 4; ++i) {
        auto *b = habit_button(habit_banner_, titles[i], 12 + i * (bw + 8), 62, bw, codes[i],
                               habit_banner_ids_.front(), 0, i == 0 ? Green : Sheet);
        lv_obj_remove_event_cb(b, action_callback);
        habit_notice_actions_[i + 1] = {this, codes[i], habit_banner_ids_.front()};
        lv_obj_add_event_cb(b, action_callback, LV_EVENT_CLICKED, &habit_notice_actions_[i + 1]);
        if (habit_waiting_ && i < 3)
            lv_obj_add_state(b, LV_STATE_DISABLED);
    }
}
void AppUi::update_habit_notices() {
    const bool critical = overlay_ != Overlay::None || habit_critical() || view_ == View::Editor ||
                          view_ == View::SearchEditor || view_ == View::ClockSetup ||
                          view_ == View::DictationSetup || dictation_.busy() || assistant_voice_.busy() ||
                          home_rotation_active_;
    const auto mission_due = service_.due_reminders();
    habits_.allow_audio(!critical && mission_due.empty());
    if (critical || !mission_due.empty() || !habits_.can_present()) {
        if (habit_banner_) {
            lv_obj_delete(habit_banner_);
            habit_banner_ = nullptr;
        }
        if (critical || !mission_due.empty())
            habits_.cancel_audio();
        return;
    }
    const auto c = habit_context();
    const int64_t now = esp_timer_get_time();
    if (!c.valid) {
        habit_banner_until_ = 0;
        render_habit_banner();
        return;
    }
    if (habits_snapshot_.notice_sequence && habits_snapshot_.notice_sequence != habit_notice_seen_) {
        habit_notice_seen_ = habits_snapshot_.notice_sequence;
        if (habits_snapshot_.presentation_day == civil_day(c.utc, c.offset_minutes)) {
            habit_banner_ids_ = habits_snapshot_.presentation_ids;
            habit_banner_until_ = now + 8000000;
            if (screen_off_)
                set_screen_off(false);
            if (!habits_snapshot_.pending.empty() &&
                habits_snapshot_.presentation_mode != HabitNoticeMode::Visual) {
                HabitCommand sound;
                sound.operation = HabitOperation::TestAudio;
                sound.value = static_cast<int>(habits_snapshot_.presentation_mode);
                habits_.post(sound);
            }
        }
    }
    if (!habits_snapshot_.unannounced.empty() && now >= habit_notice_retry_ && !habit_waiting_ &&
        !habits_snapshot_.read_only) {
        HabitCommand command;
        command.operation = HabitOperation::Announce;
        command.ids = habits_snapshot_.unannounced;
        command.sequence = ++habit_sequence_;
        if (habits_.post(command))
            habit_notice_retry_ = now + 30000000;
    }
    if (!render_pending_)
        render_habit_banner();
}
} // namespace el_sistema
