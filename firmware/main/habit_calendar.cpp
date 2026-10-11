#include "habit_calendar.hpp"
#include "fonts/fonts.h"
#include "theme.hpp"
namespace el_sistema {
void HabitCalendar::clicked(lv_event_t *event) {
    auto *hit = static_cast<Hit *>(lv_event_get_user_data(event));
    if (hit && hit->owner && hit->owner->selected_)
        hit->owner->selected_(hit->day);
}
lv_obj_t *HabitCalendar::create(lv_obj_t *parent, int x, int y, const Habit &habit, CivilDay first, CivilDay today,
                                bool valid, std::function<void(CivilDay)> callback) {
    selected_ = std::move(callback);
    auto *group = lv_obj_create(parent);
    lv_obj_set_pos(group, x, y);
    lv_obj_set_size(group, Width, Height);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(group, 0, 0);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(group, LV_OBJ_FLAG_CLICKABLE);
    const auto text = [&](const std::string &s, int tx, int ty, int width, uint32_t color) {
        auto *label = lv_label_create(group);
        lv_label_set_text(label, s.c_str());
        lv_obj_set_pos(label, tx, ty);
        lv_obj_set_width(label, width);
        lv_obj_set_style_text_font(label, &font_rajdhani_20, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        return label;
    };
    for (int col = 0; col < 12; ++col) {
        const auto monday = first + col * 7;
        const auto date = day_text(monday);
        const bool partial =
            habit.start_day != week_start(habit.start_day) && monday == week_start(habit.start_day);
        const std::string heading = date.substr(8, 2) + "/" + date.substr(5, 2) + "\n" +
                                    std::to_string(weekly_done(habit, monday)) + "/" +
                                    std::to_string(weekly_goal(habit, monday)) + (partial ? "*" : "");
        auto *label = text(heading, 48 + col * Step, 0, Cell, theme::HabitInk);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    }
    const char *weekdays[] = {"L", "M", "X", "J", "V", "S", "D"};
    for (int row = 0; row < 7; ++row) {
        text(weekdays[row], 24, 54 + row * Step + 12, 20, theme::HabitInk);
        for (int col = 0; col < 12; ++col) {
            const size_t index = row * 12 + col;
            const CivilDay day = first + col * 7 + row;
            const bool enabled = valid && day >= habit.start_day && day <= today, done = has_done(habit, day);
            auto *button = lv_button_create(group);
            cells_[index] = button;
            hits_[index] = {this, day};
            lv_obj_set_pos(button, 48 + col * Step, 54 + row * Step);
            lv_obj_set_size(button, Cell, Cell);
            lv_obj_set_style_bg_color(button,
                                      lv_color_hex(done      ? theme::HabitGreen
                                                   : enabled ? theme::HabitSheet
                                                             : theme::HabitPaper),
                                      0);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(button, lv_color_hex(day == today ? theme::HabitBlue : theme::HabitInk),
                                          0);
            lv_obj_set_style_border_width(button, day == today ? 3 : 2, 0);
            lv_obj_set_style_radius(button, 2, 0);
            lv_obj_set_style_shadow_width(button, 0, 0);
            lv_obj_set_style_pad_all(button, 0, 0);
            lv_obj_remove_flag(button, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(button, lv_color_hex(done ? theme::HabitBlue : theme::HabitPaper),
                                      LV_STATE_PRESSED);
            lv_obj_set_style_border_color(button, lv_color_hex(theme::HabitBlue), LV_STATE_PRESSED);
            if (!enabled) {
                lv_obj_add_state(button, LV_STATE_DISABLED);
                lv_obj_set_style_opa(button, LV_OPA_COVER, LV_STATE_DISABLED);
            }
            auto *label = lv_label_create(button);
            lv_label_set_text(label, done ? "X" : enabled ? "·" : "-");
            lv_obj_set_style_text_font(label, &font_rajdhani_20, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(done ? 0xFFFFFF : theme::HabitInk), 0);
            lv_obj_center(label);
            lv_obj_add_event_cb(button, clicked, LV_EVENT_CLICKED, &hits_[index]);
        }
    }
    return group;
}
} // namespace el_sistema
