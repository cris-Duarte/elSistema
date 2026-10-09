#pragma once

#include "hardware.hpp"
#include "home_scene.hpp"
#include "home_renderer.hpp"
#include "hourglass.hpp"
#include "hourglass_renderer.hpp"
#include "radar.hpp"
#include "radar_renderer.hpp"
#include "tetris.hpp"
#include "tetris_renderer.hpp"
#include "maze.hpp"
#include "maze_renderer.hpp"
#include "modules.hpp"
#include "assistant.hpp"
#include "assistant_voice.hpp"
#include "dictation.hpp"
#include "model.hpp"
#include "repository.hpp"
#include "services.hpp"
#include "theme.hpp"

#include <memory>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "lvgl.h"

namespace el_sistema {

class AppUi {
public:
    AppUi(AppState& state, ApplicationService& service, MissionRepository& repository, ClockService& clock,
          OrientationService& orientation, ButtonService& button, DictationService& dictation,
          PowerService& power, AssistantService& assistant, AssistantVoiceService& assistant_voice,
          HourglassService& hourglass, RadarService& radar, TetrisService& tetris, MazeService& maze);
    void start(lv_display_t* display);
#ifdef EL_SISTEMA_RADAR_SMOKE
    void radar_validation_view(bool open, int rotation = 1);
#endif

private:
    enum class View { Home, Dashboard, Assistant, History, Editor, ClockSetup, SearchEditor, DictationSetup,
                      HourglassSetup, Hourglass, Radar, Tetris, Maze };
    enum class Overlay { None, Tutorial, Reward, Reminder, ConfirmDelete, ConfirmForgetNetwork, MissionDetail, Info,
                         Dictation, ConfirmDictationReplace, ConfirmHourglassReset, RadarPick, ConfirmTetrisNew };
    enum class KeyboardPage { Letters, Numbers, Symbols };
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
        OpenDictation,
        DictationStop,
        DictationCancel,
        DictationRetry,
        DictationAccept,
        ConfirmDictationReplace,
        OpenConnectivity,
        ConnectivityTab,
        ScanNetworks,
        OpenHiddenNetwork,
        CancelNetworkEditor,
        ConnectAndSaveNetwork,
        ConnectKnownNetwork,
        ForgetNetwork,
        ConfirmForgetNetwork,
        MoveNetworkPriority,
        ReconnectNetworks,
        RefreshServers,
        SelectServer,
        TestWhisperServer,
        SelectNetwork,
        FocusNetworkField,
        CloseConnectivity,
        PlayLastCapture,
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
        ToggleSidePanel,
        OpenAssistant,
        CloseAssistant,
        OpenModule,
        GoHome,
        TetrisNew, TetrisConfirmNew, TetrisContinue, TetrisPause, TetrisSound,
        TetrisMusic, TetrisEffects, TetrisVolume,
        MazeStart, MazeResume, MazePause, MazeCalibrate, MazeSelect,
        RadarFilter,
        RadarPause,
        RadarSound,
        RadarSelect,
        RadarPage,
        HourglassPreset,
        HourglassStart,
        HourglassToggleControls,
        HourglassPauseResume,
        HourglassAskReset,
        HourglassReset,
        HourglassDuration,
        HourglassCancelSetup,
        AssistantTap,
        AssistantDemo,
        AssistantCelebrate,
        AssistantVoiceStart,
        AssistantVoiceStop,
        AssistantVoiceCancel,
        AssistantVoiceRetry,
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
    static void home_animation_callback(lv_timer_t* timer);
    static void home_touch_callback(lv_event_t* event);
    static void hourglass_animation_callback(lv_timer_t* timer);
    static void radar_animation_callback(lv_timer_t* timer);
    static void radar_touch_callback(lv_event_t* event);
    static void home_rotation_exec(void* ui, int32_t progress);
    static void home_rotation_complete(lv_anim_t* animation);
    static void render_callback(void* user_data);
    void dispatch(const Action& action);
    void tick();
    void schedule_render();

    void render();
    void render_home();
    void render_tetris();
    void render_maze();
    void update_maze();
    void leave_maze();
    static void maze_animation_callback(lv_timer_t* timer);
    void update_tetris();
    void leave_tetris();
    static void tetris_touch_callback(lv_event_t* event);
    static void tetris_animation_callback(lv_timer_t* timer);
    void render_radar();
    void update_radar();
    void render_hourglass_setup();
    void render_hourglass();
    void update_hourglass();
    void leave_hourglass(View target = View::Home);
    void render_dashboard();
    void render_assistant();
    void render_landscape_dashboard();
    void render_portrait_dashboard();
    void render_history();
    void render_editor(bool search_only = false);
    void render_clock_setup();
    void render_dictation_setup();
    void render_overlay();
    void render_screen_off();

    void render_header(bool compact = false);
    void render_statusbar();
    void render_active_panel(lv_obj_t* parent, int width, int height);
    void render_focus_list(lv_obj_t* parent, int width, int height);
    void render_backlog_list(lv_obj_t* parent, int width, int height);
    void render_mission_row(lv_obj_t* parent, const Mission& mission, bool focus, int width);
    void refresh_assistant_visual();
    void update_home_animation();
    void start_home_rotation();
    void cancel_home_rotation();
    void open_module(ModuleId module);

    lv_obj_t* panel(lv_obj_t* parent, int x, int y, int width, int height, bool scrollable = false);
    lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t color);
    lv_obj_t* button(lv_obj_t* parent, const char* text, int x, int y, int width, int height, ActionCode code,
                     MissionId id = 0, int value = 0, const char* action_text = nullptr,
                     uint32_t background = theme::SurfaceAlt, uint32_t border = theme::Border);
    lv_obj_t* clock_roller(int x, int y, int width, const std::string& options, int selected);
    lv_obj_t* spanish_keyboard(int x, int y, int width, int height);
    void update_keyboard_map();
    void handle_key(const char* text);
    void set_active_textarea(lv_obj_t* target);
    Action* make_action(ActionCode code, MissionId id = 0, int value = 0, const char* text = nullptr);

    void open_editor(MissionId id);
    void capture_editor();
    void capture_network_credentials();
    void apply_dictation_candidate();
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
    // Textos de la barra superior. Se recalculan en cada tick y solo tocan los
    // widgets cuando cambian, para no reconstruir la pantalla cada segundo.
    std::string status_clock_text() const;
    std::string status_net_text() const;
    std::string status_power_text();
    void refresh_statusbar();

    AppState& state_;
    ApplicationService& service_;
    MissionRepository& repository_;
    ClockService& clock_;
    OrientationService& orientation_;
    ButtonService& button_;
    DictationService& dictation_;
    PowerService& power_;
    AssistantService& assistant_;
    AssistantVoiceService& assistant_voice_;
    HourglassService& hourglass_;
    RadarService& radar_;
    TetrisService& tetris_;
    MazeService& maze_;
    MazeSnapshot maze_snapshot_{};
    std::unique_ptr<MazeRenderer> maze_renderer_;
    lv_timer_t* maze_timer_=nullptr;
    lv_obj_t* maze_status_=nullptr;
    int maze_return_rotation_=1;
    TetrisSnapshot tetris_snapshot_{};
    std::unique_ptr<TetrisRenderer> tetris_renderer_;
    TetrisGesture tetris_gesture_;
    lv_timer_t* tetris_timer_ = nullptr;
    lv_obj_t* tetris_score_ = nullptr;
    lv_obj_t* tetris_level_ = nullptr;
    lv_obj_t* tetris_record_ = nullptr;
    std::array<lv_obj_t*,4> tetris_preview_{};
    lv_obj_t* tetris_notice_ = nullptr;
    bool tetris_playing_ = false, tetris_sound_menu_ = false;
    TetrisState tetris_last_state_ = TetrisState::Ready;
    int tetris_return_rotation_ = 1;
    uint8_t tetris_preview_kind_ = 255;
    int64_t tetris_level_until_ = 0;
    uint32_t tetris_last_level_ = 1, tetris_frames_ = 0;
    int64_t tetris_fps_since_ = 0;
    lv_display_t* display_ = nullptr;
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* timer_label_ = nullptr;
    lv_obj_t* active_status_label_ = nullptr;
    lv_obj_t* status_clock_label_ = nullptr;
    lv_obj_t* status_net_label_ = nullptr;
    lv_obj_t* status_power_label_ = nullptr;
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
    lv_obj_t* ssid_textarea_ = nullptr;
    lv_obj_t* password_textarea_ = nullptr;
    lv_obj_t* dictation_status_label_ = nullptr;
    lv_obj_t* dictation_time_label_ = nullptr;
    lv_obj_t* dictation_level_bar_ = nullptr;
    lv_obj_t* assistant_character_ = nullptr;
    lv_obj_t* assistant_aura_ = nullptr;
    lv_obj_t* assistant_eye_left_ = nullptr;
    lv_obj_t* assistant_eye_right_ = nullptr;
    lv_obj_t* assistant_mouth_ = nullptr;
    lv_obj_t* assistant_core_ = nullptr;
    lv_obj_t* assistant_state_label_ = nullptr;
    lv_obj_t* assistant_message_label_ = nullptr;
    lv_obj_t* assistant_level_bar_ = nullptr;
    lv_obj_t* home_time_label_ = nullptr;
    lv_obj_t* home_date_label_ = nullptr;
    lv_obj_t* home_sandbox_ = nullptr;
    lv_obj_t* home_header_group_ = nullptr;
    lv_obj_t* home_modules_group_ = nullptr;
    std::unique_ptr<HomeRenderer> home_renderer_;
    int64_t home_last_frame_us_ = 0;
    int64_t home_fps_since_us_ = 0;
    uint32_t home_frame_count_ = 0;
    lv_timer_t* tick_timer_ = nullptr;
    lv_timer_t* home_animation_timer_ = nullptr;
    lv_timer_t* hourglass_animation_timer_ = nullptr;
    lv_timer_t* radar_animation_timer_ = nullptr;
    std::unique_ptr<RadarRenderer> radar_renderer_;
    RadarSnapshot radar_snapshot_;
    std::vector<RadarPoint> radar_points_;
    std::vector<uint32_t> radar_hits_;
    bool radar_visible_ = false;
#ifdef EL_SISTEMA_RADAR_SMOKE
    bool radar_validation_active_ = false;
#endif
    int radar_page_ = 0, radar_size_ = 480;
    int64_t radar_next_update_ms_ = 0, radar_fps_since_ms_ = 0;
    uint32_t radar_frames_ = 0;
    lv_obj_t* radar_status_ = nullptr;
    lv_obj_t* radar_detail_ = nullptr;
    lv_obj_t* radar_chart_ = nullptr;
    lv_chart_series_t* radar_series_ = nullptr;
    lv_obj_t* radar_pause_button_ = nullptr;
    lv_obj_t* radar_sound_button_ = nullptr;
    lv_obj_t* radar_page_label_ = nullptr;
    std::array<lv_obj_t*, 12> radar_rows_ {};
    std::array<lv_obj_t*, 3> radar_filter_buttons_ {};
    std::array<Action*, 12> radar_row_actions_ {};
    lv_obj_t* hourglass_time_label_ = nullptr;
    lv_obj_t* hourglass_controls_ = nullptr;
    lv_obj_t* hourglass_primary_ = nullptr;
    lv_obj_t* hourglass_error_label_ = nullptr;
    lv_obj_t* hourglass_duration_roller_ = nullptr;
    std::unique_ptr<HourglassRenderer> hourglass_renderer_;
    bool hourglass_controls_open_ = false;
    bool hourglass_config_from_session_ = false;
    bool hourglass_start_pending_ = false;
    uint32_t hourglass_pending_command_ = 0;
    bool hourglass_visible_ = false;
    uint16_t hourglass_selected_minutes_ = 25;
    int hourglass_return_rotation_ = 1;
    int64_t hourglass_controls_since_us_ = 0;
    int64_t hourglass_fps_since_us_ = 0;
    uint32_t hourglass_frames_ = 0;
    uint32_t hourglass_last_frame_ = UINT32_MAX;
    uint32_t hourglass_last_generation_ = UINT32_MAX;
    std::string hourglass_last_time_;
    std::string hourglass_last_error_;
    // Varias acciones pueden solicitar un redibujado antes de que LVGL ejecute
    // la llamada asincrona. Mantener una sola pendiente evita reconstruir la
    // jerarquia dos veces y liberar objetos que aun estan siendo dibujados.
    bool render_pending_ = false;

    View view_ = View::Home;
    Overlay overlay_ = Overlay::None;
    bool screen_off_ = false;
    MissionId editing_id_ = 0;
    MissionId overlay_mission_id_ = 0;
    MissionId suggested_next_id_ = 0;
    MissionRank editor_rank_ = MissionRank::E;
    bool editor_reminder_enabled_ = true;
    uint16_t editor_reminder_minutes_ = 5;
    bool keyboard_upper_ = false;
    KeyboardPage keyboard_page_ = KeyboardPage::Letters;
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
    bool dictation_from_editor_ = false;
    View connectivity_return_view_ = View::Dashboard;
    std::string dictation_candidate_;
    NetworkCredentials network_editor_;
    bool network_editor_open_ = false;
    bool network_editor_secured_ = true;
    bool network_connect_pending_ = false;
    int pending_forget_network_ = -1;
    int connectivity_tab_ = 0;
    uint32_t last_connectivity_generation_ = 0;
    uint32_t last_ble_generation_ = 0;
    uint32_t last_assistant_generation_ = 0;
    uint32_t last_assistant_voice_generation_ = 0;
    uint32_t assistant_animation_tick_ = 0;
    HomeScene home_scene_;
    bool home_scene_initialized_ = false;
    int home_scene_rotation_ = -1;
    bool home_motion_enabled_ = false;
    struct RotatingWidget {
        lv_obj_t* object = nullptr;
        int start_x = 0, start_y = 0;
        int end_x = 0, end_y = 0;
    };
    std::vector<ScreenPoint> home_rotation_sources_;
    std::vector<RotatingWidget> home_rotating_widgets_;
    int home_rotation_turns_ = 0;
    bool home_rotation_pending_ = false;
    bool home_rotation_active_ = false;
    bool home_rotation_supported_ = false;
    int last_rotation_lock_ = -1;
    int assistant_character_base_y_ = 0;
    DictationPhase last_dictation_phase_ = DictationPhase::Unconfigured;
    std::string last_dictation_message_;
    // La red y la energía se sondean cada pocos segundos; el reloj, cada tick.
    // Se guarda el último texto para no tocar los widgets sin cambios.
    std::string last_status_clock_;
    std::string last_status_net_;
    std::string last_status_power_;
    int64_t last_status_second_ = -1;
    int portrait_tab_ = 0;
    // Panel lateral del dashboard horizontal: -1 colapsado, 0 enfoque, 1 backlog.
    int side_panel_ = -1;
    int current_rotation_ = 1;
    int last_checkpoint_second_ = -1;
    std::string overlay_title_;
    std::string overlay_body_;
    std::vector<std::unique_ptr<Action>> actions_;
    std::vector<std::pair<MissionId, lv_obj_t*>> reminder_labels_;
};

}  // namespace el_sistema
