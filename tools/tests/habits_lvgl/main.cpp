#include "fonts/fonts.h"
#include "habit_calendar.hpp"
#include "theme.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace el_sistema;
static void flush(lv_display_t *d, const lv_area_t *, uint8_t *) {
    lv_display_flush_ready(d);
}
int main() {
    lv_init();
    for (const auto size : {std::pair<int, int>{720, 1280}, {1280, 720}}) {
        auto *display = lv_display_create(size.first, size.second);
        std::vector<uint8_t> buffer(size.first * 100 * 4);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
        lv_display_set_buffers(display, buffer.data(), nullptr, buffer.size(), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, flush);
        auto *screen = lv_screen_active();
        // LVGL 9.2: captions inherit disabled contrast from their button.
        auto *button = lv_button_create(screen);
        auto *caption = lv_label_create(button);
        lv_obj_set_style_text_color(caption, lv_color_hex(0xFFFFFF), 0);
        lv_obj_remove_local_style_prop(caption, LV_STYLE_TEXT_COLOR, 0);
        lv_obj_set_style_text_color(button, lv_color_hex(theme::HabitInk), 0);
        lv_obj_set_style_text_color(button, lv_color_hex(theme::HabitMuted), LV_STATE_DISABLED);
        assert(lv_color_eq(lv_obj_get_style_text_color(caption, 0), lv_color_hex(theme::HabitInk)));
        lv_obj_add_state(button, LV_STATE_DISABLED);
        assert(lv_color_eq(lv_obj_get_style_text_color(caption, 0), lv_color_hex(theme::HabitMuted)));
        lv_obj_delete(button);
        Habit habit;
        habit.id = 1;
        habit.name = "Ejercicio";
        habit.start_day = date_day(2026, 8, 19);
        habit.goals = {{week_start(habit.start_day), 5}, {date_day(2026, 10, 5), 7}};
        habit.done = {date_day(2026, 10, 7)};
        const auto today = date_day(2026, 10, 9), first = week_start(today) - 77;
        int clicks = 0;
        CivilDay selected = 0;
        HabitCalendar calendar;
        const int detail_x = size.first == 720 ? 16 : 508;
        auto *group = calendar.create(screen, detail_x, 230, habit, first, today, true, [&](CivilDay day) {
            ++clicks;
            selected = day;
        });
        lv_obj_update_layout(group);
        for (size_t i = 0; i < 84; ++i) {
            auto *cell = calendar.cell(i);
            lv_area_t bounds{};
            lv_obj_get_coords(cell, &bounds);
            assert(bounds.x2 - bounds.x1 + 1 == 48 && bounds.y2 - bounds.y1 + 1 == 48);
            assert(bounds.x1 >= 0 && bounds.x2 < size.first && bounds.y1 >= 0 && bounds.y2 < size.second);
            const auto day = first + int(i % 12) * 7 + int(i / 12);
            assert(bool(lv_obj_has_state(cell, LV_STATE_DISABLED)) == (day < habit.start_day || day > today));
            if (day == today) {
                lv_obj_send_event(cell, LV_EVENT_CLICKED, nullptr);
                assert(selected == today && clicks == 1);
            }
        }
        assert(clicks == 1);
        for (const char *string :
             {"REGISTRAR INTERRUPCIÓN", "LA PRÓXIMA SEMANA", "CONTINUOS", "OMITIR HOY", "HÁBITOS"}) {
            auto *label = lv_label_create(screen);
            lv_obj_set_style_text_font(label, &font_rajdhani_20, 0);
            lv_label_set_text(label, string);
            lv_obj_update_layout(label);
            assert(lv_obj_get_width(label) < 300);
            lv_obj_delete(label);
        }
        for (const lv_font_t *font : {&font_rajdhani_20, &font_rajdhani_40, &font_rajdhani_64, &font_poppins_16}) {
            lv_font_glyph_dsc_t glyph{};
            for (uint32_t cp : {0x00E1U, 0x00E9U, 0x00EDU, 0x00F3U, 0x00FAU, 0x00F1U, 0x00FCU})
                assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0));
        }
        for (int i = 0; i < 10; ++i) {
            lv_tick_inc(33);
            lv_timer_handler();
        }
        lv_obj_clean(screen);
        lv_display_delete(display);
        std::printf("Hábitos LVGL %d×%d: 84 casillas de 48 px, estados, tacto y fuentes españolas OK\n",
                    size.first, size.second);
    }
    lv_deinit();
}
