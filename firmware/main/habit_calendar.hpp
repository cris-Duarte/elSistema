#pragma once
#include "habit_model.hpp"
#include "lvgl.h"
#include <array>
#include <functional>
namespace el_sistema {
// Renderer-owned event data has fixed lifetime until its LVGL group is deleted.
class HabitCalendar {
  public:
    static constexpr int Cell = 48, Step = 50, Width = 648, Height = 414;
    lv_obj_t *create(lv_obj_t *parent, int x, int y, const Habit &habit, CivilDay first, CivilDay today,
                     bool clock_valid, std::function<void(CivilDay)> selected);
    lv_obj_t *cell(size_t index) const { return index < cells_.size() ? cells_[index] : nullptr; }

  private:
    struct Hit {
        HabitCalendar *owner = nullptr;
        CivilDay day = 0;
    };
    static void clicked(lv_event_t *event);
    std::array<Hit, 84> hits_{};
    std::array<lv_obj_t *, 84> cells_{};
    std::function<void(CivilDay)> selected_;
};
} // namespace el_sistema
