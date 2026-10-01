#pragma once

#include "hardware.hpp"
#include "model.hpp"
#include "repository.hpp"
#include "services.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lvgl.h"

namespace el_sistema {

class AppUi {
public:
    AppUi(AppState& state, ApplicationService& service, MissionRepository& repository, ClockService& clock,
          OrientationService& orientation, ButtonService& button);
    void start(lv_display_t* display);

private:
    enum class View { Dashboard, History, Editor, ClockSetup, SearchEditor };
    enum class Overlay { None, Tutorial, Reward, Reminder, ConfirmDelete, MissionDetail, Info };
    enum class ActionCode {
        NewMission,
        EditMission,
        SaveMission,
        CancelEditor,
        AddFocus,
        StartMission,
        PauseMission,
        CompleteMission,
        RemoveFocus,
        MoveUp,
        MoveDown,
        OpenHistory,
        CloseHistory,
        OpenSearch,
        ApplySearch,
        CycleRankFilter,
        CycleDateFilter,
        ToggleOrder,
        ShowDetail,
        AskDelete,
        ConfirmDelete,
        CloseOverlay,
        StartSuggested,
        ReminderResume,
        ReminderSnooze,
        ReminderCancel,
        SaveClock,
        ClockOffsetMinus,
        ClockOffsetPlus,
        FinishTutorial,
        DictationInfo,
        KeyboardKey,
        SelectRank,
        ReminderMinus,
        ReminderPlus,
        ReminderToggle,
        FocusTitle,
        FocusDescription,
        RefreshClockDays,
        WakeScreen,
        PowerOffDevice,
    };

    struct Action {
        AppUi* ui = nullptr;
        ActionCode code = ActionCode::CloseOverlay;
        MissionId id = 0;
        int value = 0;
        const char* text = nullptr;
    };

    static void action_callback(lv_event_t* event);
    static void tick_callback(lv_timer_t* timer);
    static void render_callback(void* user_data);
    void dispatch(const Action& action);
    void tick();
    void schedule_render();

    void render();
    void render_dashboard();
    void render_landscape_dashboard();
    void render_portrait_dashboard();
    void render_history();
    void render_editor(bool search_only = false);
    void render_clock_setup();
    void render_overlay();
    void render_screen_off();

    void render_header(bool compact = false);
    void render_active_panel(lv_obj_t* parent, int width, int height);
    void render_focus_list(lv_obj_t* parent, int width, int height);
    void render_backlog_list(lv_obj_t* parent, int width, int height);
    void render_mission_row(lv_obj_t* parent, const Mission& mission, bool focus, int width);

    lv_obj_t* panel(lv_obj_t* parent, int x, int y, int width, int height, bool scrollable = false);
    lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color);
    lv_obj_t* button(lv_obj_t* parent, const char* text, int x, int y, int width, int height, ActionCode code,
                     MissionId id = 0, int value = 0, const char* action_text = nullptr, uint32_t background = 0x111B34,
                     uint32_t border = 0x2B4169);
    lv_obj_t* clock_roller(int x, int y, int width, const std::string& options, int selected);
    lv_obj_t* spanish_keyboard(int x, int y, int width, int height);
    void handle_key(const char* text);
    void set_active_textarea(lv_obj_t* target);
    Action* make_action(ActionCode code, MissionId id = 0, int value = 0, const char* text = nullptr);

    void open_editor(MissionId id);
    void capture_editor();
    void seed_clock_defaults();
    void capture_clock();
    void update_clock_days();
    void show_info(const std::string& title, const std::string& body);
    void save_clock();
    void set_rotation(int rotation);
    void set_screen_off(bool off);
    void power_off_device();
    bool is_portrait() const;
    std::string duration_text(uint32_t seconds) const;
    std::string reminder_text(const Mission& mission) const;
    std::string truncate(const std::string& text, size_t max_chars) const;

    AppState& state_;
    ApplicationService& service_;
    MissionRepository& repository_;
    ClockService& clock_;
    OrientationService& orientation_;
    ButtonService& button_;
    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* timer_label_ = nullptr;
    lv_obj_t* active_status_label_ = nullptr;
    lv_obj_t* title_textarea_ = nullptr;
    lv_obj_t* description_textarea_ = nullptr;
    lv_obj_t* keyboard_ = nullptr;
    lv_obj_t* clock_day_roller_ = nullptr;
    lv_obj_t* clock_month_roller_ = nullptr;
    lv_obj_t* clock_year_roller_ = nullptr;
    lv_obj_t* clock_hour_roller_ = nullptr;
    lv_obj_t* clock_minute_roller_ = nullptr;
    lv_obj_t* reminder_value_label_ = nullptr;
    lv_obj_t* active_textarea_ = nullptr;
    lv_timer_t* tick_timer_ = nullptr;

    View view_ = View::Dashboard;
    Overlay overlay_ = Overlay::None;
    bool screen_off_ = false;
    MissionId editing_id_ = 0;
    MissionId overlay_mission_id_ = 0;
    MissionId suggested_next_id_ = 0;
    MissionRank editor_rank_ = MissionRank::E;
    bool editor_reminder_enabled_ = true;
    uint16_t editor_reminder_minutes_ = 5;
    bool keyboard_upper_ = false;
    int clock_day_ = 1;
    int clock_month_ = 1;
    int clock_year_ = 2024;
    int clock_hour_ = 0;
    int clock_minute_ = 0;
    int clock_day_count_ = 0;
    bool clock_defaults_ready_ = false;
    std::string editor_title_;
    std::string editor_description_;
    std::string search_query_;
    int rank_filter_ = -1;
    int date_filter_days_ = 0;
    bool newest_first_ = true;
    int portrait_tab_ = 0;
    int current_rotation_ = 1;
    int last_checkpoint_second_ = -1;
    std::string overlay_title_;
    std::string overlay_body_;
    std::vector<std::unique_ptr<Action>> actions_;
    std::vector<std::pair<MissionId, lv_obj_t*>> reminder_labels_;
};

}  // namespace el_sistema
