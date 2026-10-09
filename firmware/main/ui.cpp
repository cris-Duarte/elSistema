#include "ui.hpp"

#include "assets/assistant_character.h"
#include "fonts/fonts.h"
#include "radar_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "bsp/m5stack_tab5.h"

namespace el_sistema {
namespace {

constexpr const char* TAG_UI = "ui";

// Transformed LVGL objects need complete ARGB layers. The default 64 KiB
// pool fits the static UI, but not even a rotated clock: LVGL otherwise
// retries the impossible allocation forever inside its draw dispatcher.
// Keep a dedicated lifetime pool in PSRAM, without changing BSP framebuffers.
bool prepare_rotation_memory()
{
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    static bool attempted = false;
    static lv_mem_pool_t pool = nullptr;
    if (!attempted) {
        attempted = true;
        constexpr size_t bytes = 2 * 1024 * 1024;
        void* memory = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory != nullptr) {
            pool = lv_mem_add_pool(memory, bytes);
            if (pool == nullptr) heap_caps_free(memory);
        }
        if (pool != nullptr) ESP_LOGI(TAG_UI, "LVGL: reserva de 2 MiB en PSRAM para capas de rotación");
        else ESP_LOGW(TAG_UI, "Sin memoria para giro animado; se conserva la rotación instantánea");
    }
    return pool != nullptr;
#else
    return false; // Reassess the allocator budget before enabling transforms.
#endif
}

// Alias cortos de los tokens de identidad (main/theme.hpp). Tener un solo punto
// de definicion deja el diff pequeno y evita colores literales dispersos.
constexpr uint32_t BG = theme::Background;
constexpr uint32_t PANEL = theme::Surface;
constexpr uint32_t PANEL_ALT = theme::SurfaceAlt;
constexpr uint32_t LINE = theme::Border;
constexpr uint32_t CYAN = theme::Accent;
constexpr uint32_t ACCENT_SOFT = theme::AccentSoft;
constexpr uint32_t TEXT = theme::TextPrimary;
constexpr uint32_t MUTED = theme::TextSecondary;
constexpr uint32_t VIOLET = theme::AccentAlt;
constexpr uint32_t DANGER = theme::Danger;
constexpr uint32_t DANGER_SOFT = theme::DangerSoft;
constexpr uint32_t GOLD = theme::Gold;  // reservado a logros mayores
constexpr uint32_t SCRIM = theme::Scrim;

// Alias de fuente (main/fonts/fonts.h). Igual que con el color, un solo punto de
// definicion: cambiar de fuente es cambiar estas cuatro lineas.
const lv_font_t& FONT_BODY = font_poppins_16;      // contenido y texto corrido
const lv_font_t& FONT_LABEL = font_rajdhani_20;    // rotulos, botones, teclas
const lv_font_t& FONT_DISPLAY = font_rajdhani_28;  // titulos, mision activa, ruedas
const lv_font_t& FONT_NUMBER = font_rajdhani_40;   // titulo protagonista, nivel
const lv_font_t& FONT_HERO = font_rajdhani_128;    // cronometro de la mision activa

const lv_font_t& keyboard_font()
{
    // Conserva el tamaño Rajdhani de 40 px y añade los pictogramas de LVGL
    // que esa fuente no contiene (mayúsculas, borrar y salto de línea).
    static const lv_font_t font = [] {
        lv_font_t result = font_rajdhani_40;
        result.fallback = &lv_font_montserrat_32;
        return result;
    }();
    return font;
}

lv_color_t color(uint32_t value) { return lv_color_hex(value); }

uint32_t rank_color(MissionRank rank)
{
    switch (rank) {
        case MissionRank::E: return theme::RankE;
        case MissionRank::D: return theme::RankD;
        case MissionRank::C: return theme::RankC;
        case MissionRank::B: return theme::RankB;
        case MissionRank::A: return theme::RankA;
        case MissionRank::S: return theme::RankS;
    }
    return theme::RankC;
}

// Importancia del rango, de menor a mayor. Va como texto junto al distintivo para
// que la jerarquia no dependa solo del color (§5).
const char* rank_importance(MissionRank rank)
{
    switch (rank) {
        case MissionRank::E: return "MÍNIMA";
        case MissionRank::D: return "BAJA";
        case MissionRank::C: return "MEDIA";
        case MissionRank::B: return "ALTA";
        case MissionRank::A: return "MUY ALTA";
        case MissionRank::S: return "MÁXIMA";
    }
    return "MEDIA";
}

MissionRank rank_from_index(int index)
{
    return static_cast<MissionRank>(std::clamp(index, 0, 5));
}

void base_style(lv_obj_t* object, uint32_t background, uint32_t border = LINE)
{
    lv_obj_set_style_bg_color(object, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(object, color(border), LV_PART_MAIN);
    lv_obj_set_style_border_width(object, theme::BorderWidth, LV_PART_MAIN);
    lv_obj_set_style_radius(object, theme::RadiusControl, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
}

void place(lv_obj_t* object, int x, int y, int width, int height)
{
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
}

constexpr int kClockYearBase = 2024;
constexpr int kClockYearCount = 22;  // 2024..2045
constexpr uint8_t kClockVisibleRows = 5;

// Altura explicita de las ruedas. No se puede preguntar al widget: en LVGL 9
// lv_obj_get_height devuelve obj->coords, que todavia vale 0 durante el render.
// Rajdhani 28 tiene 30 px de linea (frente a los 35 de la Montserrat anterior),
// asi que el espaciado sube a 20 para que cada fila siga superando el minimo
// tactil: 30 + 20 = 50 px de paso.
constexpr int kClockRollerLineSpace = 20;

int clock_roller_height()
{
    return kClockVisibleRows * (lv_font_get_line_height(&FONT_DISPLAY) + kClockRollerLineSpace);
}

int days_in_month(int year, int month)
{
    static const int kLengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return kLengths[month - 1];
}

const char* month_name(int month)
{
    static const char* kNames[] = {"Enero",     "Febrero", "Marzo",      "Abril",   "Mayo",      "Junio",
                                   "Julio",     "Agosto",  "Septiembre", "Octubre", "Noviembre", "Diciembre"};
    return month >= 1 && month <= 12 ? kNames[month - 1] : kNames[0];
}

std::string numeric_options(int count)
{
    std::string options;
    char buffer[8];
    for (int value = 0; value < count; ++value) {
        if (value > 0) options.push_back('\n');
        std::snprintf(buffer, sizeof(buffer), "%02d", value);
        options += buffer;
    }
    return options;
}

std::string day_options(int days)
{
    std::string options;
    for (int day = 1; day <= days; ++day) {
        if (day > 1) options.push_back('\n');
        options += std::to_string(day);
    }
    return options;
}

std::string month_options()
{
    std::string options;
    for (int month = 1; month <= 12; ++month) {
        if (month > 1) options.push_back('\n');
        options += month_name(month);
    }
    return options;
}

std::string year_options()
{
    std::string options;
    for (int index = 0; index < kClockYearCount; ++index) {
        if (index > 0) options.push_back('\n');
        options += std::to_string(kClockYearBase + index);
    }
    return options;
}

// Fecha y hora de compilación del firmware. Se usan como valor inicial cuando el
// RTC todavía no tiene hora válida: normalmente está a minutos del flasheo, así
// que el usuario corrige el desfase en lugar de recorrer años en las ruedas.
void build_time(std::tm& value)
{
    static const char* kMonthTokens = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month_text[4] = {};
    int day = 1;
    int year = kClockYearBase;
    int hour = 0;
    int minute = 0;
    std::sscanf(__DATE__, "%3s %d %d", month_text, &day, &year);
    std::sscanf(__TIME__, "%d:%d", &hour, &minute);
    const char* found = std::strstr(kMonthTokens, month_text);
    value = {};
    value.tm_mday = day;
    value.tm_mon = (found != nullptr ? static_cast<int>(found - kMonthTokens) / 3 : 0);
    value.tm_year = year - 1900;
    value.tm_hour = hour;
    value.tm_min = minute;
}

// Teclado táctil en español. Los números, signos, acentos y caracteres de
// contraseña se separan en las páginas 123 y #+= para dejar más superficie a
// las letras. Cada fila termina en "\n"; el mapa no depende de símbolos de icono.
const char* const kKeyboardLower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ", "\n",
    LV_SYMBOL_UP, "z", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
    "123", ",", "ESPACIO", ".", LV_SYMBOL_NEW_LINE, ""
};
const char* const kKeyboardUpper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ", "\n",
    LV_SYMBOL_UP, "Z", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
    "123", ",", "ESPACIO", ".", LV_SYMBOL_NEW_LINE, ""
};
const char* const kKeyboardNumbersLower[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "_", ".", ",", ":", ";", "(", ")", "/", "@", "\n",
    "á", "é", "í", "ó", "ú", "ü", "ñ", "¿", "¡", "?", "\n",
    "ABC", "#+=", LV_SYMBOL_UP, "ESPACIO", LV_SYMBOL_BACKSPACE, LV_SYMBOL_NEW_LINE, ""
};
const char* const kKeyboardNumbersUpper[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "_", ".", ",", ":", ";", "(", ")", "/", "@", "\n",
    "Á", "É", "Í", "Ó", "Ú", "Ü", "Ñ", "¿", "¡", "?", "\n",
    "ABC", "#+=", LV_SYMBOL_UP, "ESPACIO", LV_SYMBOL_BACKSPACE, LV_SYMBOL_NEW_LINE, ""
};
const char* const kKeyboardSymbols[] = {
    "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "\n",
    "+", ",", "-", ".", "/", ":", ";", "<", "=", ">", "\n",
    "?", "@", "[", "\\", "]", "^", "_", "`", "{", "|", "\n",
    "}", "~", "123", "ABC", "ESPACIO", LV_SYMBOL_BACKSPACE, LV_SYMBOL_NEW_LINE, ""
};

// Barra superior de estado compacta con zonas táctiles de 48 px.
// Se dibuja la última para que ningún panel ni diálogo la tape.
constexpr int kStatusBarHeight = 48;

}  // namespace

AppUi::AppUi(AppState& state, ApplicationService& service, MissionRepository& repository, ClockService& clock,
             OrientationService& orientation, ButtonService& button, DictationService& dictation,
             PowerService& power, AssistantService& assistant, AssistantVoiceService& assistant_voice,
             HourglassService& hourglass, RadarService& radar, TetrisService& tetris, MazeService& maze)
    : state_(state), service_(service), repository_(repository), clock_(clock), orientation_(orientation),
      button_(button), dictation_(dictation), power_(power), assistant_(assistant), assistant_voice_(assistant_voice),
      hourglass_(hourglass), radar_(radar), tetris_(tetris), maze_(maze)
{
}

void AppUi::start(lv_display_t* display)
{
    display_ = display;
    screen_ = lv_screen_active();
    home_rotation_supported_ = prepare_rotation_memory();
    current_rotation_ = 1;
    orientation_.set_current(current_rotation_);
    if (!clock_.is_valid()) {
        seed_clock_defaults();
        view_ = View::ClockSetup;
    } else {
        view_ = View::Home;
    }
    render();
    tick_timer_ = lv_timer_create(tick_callback, 250, this);
}

void AppUi::action_callback(lv_event_t* event)
{
    auto* action = static_cast<Action*>(lv_event_get_user_data(event));
    if (action != nullptr && action->ui != nullptr) {
        if (action->ui->view_ == View::Hourglass) action->ui->hourglass_controls_since_us_ = esp_timer_get_time();
        action->ui->dispatch(*action);
    }
}

void AppUi::tick_callback(lv_timer_t* timer)
{
    auto* ui = static_cast<AppUi*>(lv_timer_get_user_data(timer));
    if (ui != nullptr) ui->tick();
}

void AppUi::home_animation_callback(lv_timer_t* timer)
{
    auto* ui = static_cast<AppUi*>(lv_timer_get_user_data(timer));
    if (ui != nullptr) ui->update_home_animation();
}

void AppUi::hourglass_animation_callback(lv_timer_t* timer)
{
    auto* ui = static_cast<AppUi*>(lv_timer_get_user_data(timer));
    if (ui != nullptr) ui->update_hourglass();
}

void AppUi::home_touch_callback(lv_event_t* event)
{
    auto* ui = static_cast<AppUi*>(lv_event_get_user_data(event));
    lv_indev_t* input = lv_indev_active();
    if (ui == nullptr || input == nullptr) return;
    lv_point_t point{};
    lv_indev_get_point(input, &point);
    ui->home_scene_.perturb(static_cast<float>(point.x), static_cast<float>(point.y));
}

void AppUi::render_callback(void* user_data)
{
    auto* ui = static_cast<AppUi*>(user_data);
    ui->render_pending_ = false;
    ui->render();
}

void AppUi::schedule_render()
{
    if (render_pending_) return;
    render_pending_ = true;
    if (lv_async_call(render_callback, this) != LV_RESULT_OK) {
        render_pending_ = false;
        ESP_LOGE(TAG_UI, "No se pudo programar el redibujado");
    }
}

AppUi::Action* AppUi::make_action(ActionCode code, MissionId id, int value, const char* text_value)
{
    auto action = std::make_unique<Action>();
    action->ui = this;
    action->code = code;
    action->id = id;
    action->value = value;
    action->text = text_value;
    Action* result = action.get();
    actions_.push_back(std::move(action));
    return result;
}

lv_obj_t* AppUi::label(lv_obj_t* parent, const char* text_value, const lv_font_t* font, uint32_t text_color)
{
    lv_obj_t* object = lv_label_create(parent);
    lv_label_set_text(object, text_value);
    lv_obj_set_style_text_font(object, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(object, color(text_color), LV_PART_MAIN);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    return object;
}

lv_obj_t* AppUi::button(lv_obj_t* parent, const char* text_value, int x, int y, int width, int height,
                        ActionCode code, MissionId id, int value, const char* action_text, uint32_t background,
                        uint32_t border)
{
    lv_obj_t* object = lv_button_create(parent);
    place(object, x, y, width, height);
    base_style(object, background, border);
    lv_obj_set_style_bg_color(object, color(VIOLET), static_cast<lv_style_selector_t>(LV_PART_MAIN) |
                                                        static_cast<lv_style_selector_t>(LV_STATE_PRESSED));
    lv_obj_add_event_cb(object, action_callback, LV_EVENT_CLICKED, make_action(code, id, value, action_text));
    lv_obj_t* caption = label(object, text_value, &FONT_LABEL, TEXT);
    lv_obj_center(caption);
    return object;
}

lv_obj_t* AppUi::panel(lv_obj_t* parent, int x, int y, int width, int height, bool scrollable)
{
    lv_obj_t* object = lv_obj_create(parent);
    place(object, x, y, width, height);
    base_style(object, PANEL);
    lv_obj_set_style_radius(object, theme::RadiusPanel, LV_PART_MAIN);
    if (scrollable) {
        lv_obj_set_scroll_dir(object, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_AUTO);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    }
    return object;
}

void AppUi::render()
{
    if (screen_ == nullptr) return;
    tetris_.set_visible(view_ == View::Tetris && !screen_off_);
    maze_.set_visible(view_ == View::Maze && !screen_off_);
    if (maze_timer_) { lv_timer_delete(maze_timer_); maze_timer_=nullptr; }
    tetris_gesture_.cancel();
    tetris_.soft_drop(false);
    if (tetris_timer_) { lv_timer_delete(tetris_timer_); tetris_timer_ = nullptr; }
    const bool radar_visible = view_ == View::Radar && !screen_off_;
    if (radar_visible_ != radar_visible && radar_.visible(radar_visible)) radar_visible_ = radar_visible;
    if (radar_animation_timer_) { lv_timer_delete(radar_animation_timer_); radar_animation_timer_ = nullptr; }
    const bool should_run_home_motion = view_ == View::Home && !screen_off_;
    const bool hourglass_view = view_ == View::Hourglass || view_ == View::HourglassSetup;
    const bool should_run_motion = (view_ == View::Home || hourglass_view || view_ == View::Maze) && !screen_off_;
    const bool should_run_hourglass = hourglass_view && !screen_off_;
    if (hourglass_visible_ != should_run_hourglass) {
        if (hourglass_.set_visible(should_run_hourglass)) hourglass_visible_ = should_run_hourglass;
    }
    cancel_home_rotation(); // No animation may retain objects being deleted.
    if (home_animation_timer_ != nullptr) {
        lv_timer_delete(home_animation_timer_);
        home_animation_timer_ = nullptr;
    }
    if (hourglass_animation_timer_ != nullptr) {
        lv_timer_delete(hourglass_animation_timer_);
        hourglass_animation_timer_ = nullptr;
    }
    if (should_run_motion != home_motion_enabled_) {
        home_motion_enabled_ = should_run_motion;
        orientation_.set_motion_enabled(home_motion_enabled_);
    }
    home_sandbox_ = nullptr;
    home_time_label_ = nullptr;
    home_date_label_ = nullptr;
    home_header_group_ = nullptr;
    home_modules_group_ = nullptr;
    hourglass_time_label_ = nullptr;
    hourglass_controls_ = nullptr;
    hourglass_primary_ = nullptr;
    hourglass_error_label_ = nullptr;
    hourglass_duration_roller_ = nullptr;
    hourglass_last_frame_ = hourglass_last_generation_ = UINT32_MAX;
    hourglass_last_time_.clear();
    hourglass_last_error_.clear();
    lv_obj_clean(screen_);
    maze_renderer_.reset();maze_status_=nullptr;
    tetris_renderer_.reset();
    tetris_score_ = tetris_level_ = tetris_record_ = tetris_notice_ = nullptr;
    tetris_preview_.fill(nullptr);
    tetris_preview_kind_ = 255;
    // Delete the image object before freeing either of its RAM canvases.
    home_renderer_.reset();
    hourglass_renderer_.reset();
    radar_renderer_.reset();
    lv_timer_set_period(lv_display_get_refr_timer(display_), should_run_home_motion ? HOME_FRAME_MS :
                        hourglass_view || radar_visible ? 30 : 33);
    actions_.clear();
    reminder_labels_.clear();
    timer_label_ = nullptr;
    active_status_label_ = nullptr;
    status_clock_label_ = nullptr;
    status_net_label_ = nullptr;
    status_power_label_ = nullptr;
    title_textarea_ = nullptr;
    description_textarea_ = nullptr;
    keyboard_ = nullptr;
    clock_day_roller_ = nullptr;
    clock_month_roller_ = nullptr;
    clock_year_roller_ = nullptr;
    clock_hour_roller_ = nullptr;
    clock_minute_roller_ = nullptr;
    reminder_value_label_ = nullptr;
    active_textarea_ = nullptr;
    ssid_textarea_ = nullptr;
    password_textarea_ = nullptr;
    dictation_status_label_ = nullptr;
    dictation_time_label_ = nullptr;
    dictation_level_bar_ = nullptr;
    assistant_character_ = nullptr;
    assistant_aura_ = nullptr;
    assistant_eye_left_ = nullptr;
    assistant_eye_right_ = nullptr;
    assistant_mouth_ = nullptr;
    assistant_core_ = nullptr;
    assistant_state_label_ = nullptr;
    assistant_message_label_ = nullptr;
    assistant_level_bar_ = nullptr;

    const bool home = view_ == View::Home && !screen_off_;
    lv_obj_set_style_bg_color(screen_, color(home || hourglass_view || view_ == View::Radar || view_ == View::Tetris || view_ == View::Maze ? theme::HomeBackground : BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen_, &FONT_BODY, LV_PART_MAIN);

    // Con la pantalla apagada solo se dibuja la capa que absorbe el tacto, para
    // que un roce no active botones invisibles. La vista se reconstruye al volver.
    if (screen_off_) {
        home_rotation_pending_ = false;
        home_rotation_sources_.clear();
        render_screen_off();
        return;
    }

    switch (view_) {
        case View::Home: render_home(); break;
        case View::Radar: render_radar(); break;
        case View::Tetris: render_tetris(); break;
        case View::Maze: render_maze(); break;
        case View::HourglassSetup: render_hourglass_setup(); break;
        case View::Hourglass: render_hourglass(); break;
        case View::Dashboard: render_dashboard(); break;
        case View::Assistant: render_assistant(); break;
        case View::History: render_history(); break;
        case View::Editor: render_editor(false); break;
        case View::SearchEditor: render_editor(true); break;
        case View::ClockSetup: render_clock_setup(); break;
        case View::DictationSetup: render_dictation_setup(); break;
    }
    if (overlay_ != Overlay::None) render_overlay();
    // La barra va la última para que ningún panel ni diálogo la tape.
    // Radar uses the full short axis; its radio status and navigation live in
    // the side/bottom controls rather than overlaying the edge-to-edge disk.
    if (view_ != View::Hourglass && view_ != View::Radar && view_ != View::Tetris && view_ != View::Maze) render_statusbar();
    if (hourglass_view) {
        hourglass_animation_timer_ = lv_timer_create(hourglass_animation_callback, 30, this);
    }
    if (home_rotation_pending_ && view_ == View::Home && overlay_ == Overlay::None && !screen_off_) {
        start_home_rotation();
    } else {
        home_rotation_pending_ = false;
        home_rotation_sources_.clear();
    }
}

void AppUi::cancel_home_rotation()
{
    lv_anim_delete(this, home_rotation_exec);
    home_rotation_active_ = false;
    home_rotating_widgets_.clear();
}

void AppUi::home_rotation_exec(void* value, int32_t progress)
{
    auto* ui = static_cast<AppUi*>(value);
    const int angle = ui->home_rotation_turns_ * 900 * (1024 - progress) / 1024;
    for (const RotatingWidget& widget : ui->home_rotating_widgets_) {
        lv_obj_set_pos(widget.object,
                       widget.start_x + (widget.end_x - widget.start_x) * progress / 1024,
                       widget.start_y + (widget.end_y - widget.start_y) * progress / 1024);
        lv_obj_set_style_transform_rotation(widget.object, angle, LV_PART_MAIN);
    }
}

void AppUi::home_rotation_complete(lv_anim_t* animation)
{
    auto* ui = static_cast<AppUi*>(animation->var);
    home_rotation_exec(ui, 1024);
    ui->home_rotating_widgets_.clear();
    ui->home_rotation_active_ = false;
}

void AppUi::start_home_rotation()
{
    home_rotation_pending_ = false;
    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    if (!home_rotation_supported_ || memory.free_size < 1024 * 1024 || memory.free_biggest_size < 512 * 1024) {
        home_rotation_sources_.clear();
        ESP_LOGW(TAG_UI, "Giro sin animación por memoria LVGL insuficiente");
        return;
    }
    lv_obj_update_layout(screen_);
    size_t index = 0;
    for (uint32_t i = 0; i < lv_obj_get_child_count(screen_); ++i) {
        lv_obj_t* object = lv_obj_get_child(screen_, i);
        if (object == home_sandbox_) continue;
        if (index >= home_rotation_sources_.size()) break;
        const ScreenPoint source = home_rotation_sources_[index++];
        const int width = lv_obj_get_width(object), height = lv_obj_get_height(object);
        home_rotating_widgets_.push_back({object,
            static_cast<int>(std::lround(source.x)) - width / 2,
            static_cast<int>(std::lround(source.y)) - height / 2,
            lv_obj_get_x(object), lv_obj_get_y(object)});
        lv_obj_set_style_transform_pivot_x(object, width / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(object, height / 2, LV_PART_MAIN);
    }
    home_rotation_sources_.clear();
    if (home_rotating_widgets_.empty()) return;
    home_rotation_active_ = true;
    home_rotation_exec(this, 0); // Compensate the display turn before its first flush.
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, this);
    lv_anim_set_values(&animation, 0, 1024);
    lv_anim_set_duration(&animation, 320);
    lv_anim_set_exec_cb(&animation, home_rotation_exec);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, home_rotation_complete);
    lv_anim_start(&animation);
}

void AppUi::render_statusbar()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    lv_obj_t* bar = lv_obj_create(screen_);
    place(bar, 0, 0, width, kStatusBarHeight);
    const bool home = view_ == View::Home && !screen_off_;
    lv_obj_set_style_bg_color(bar, color(home ? theme::HomeSurface : PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, home ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, color(home ? theme::HomeInk : LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, home ? 0 : 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    last_status_clock_ = status_clock_text();
    last_status_net_ = status_net_text();
    last_status_power_ = status_power_text();
    last_status_second_ = clock_.now_utc();
    lv_obj_t* status_parent = home ? bar : screen_;

    if (!home) {
        status_clock_label_ = label(screen_, last_status_clock_.c_str(), &FONT_BODY, TEXT);
        lv_obj_set_pos(status_clock_label_, 20, (kStatusBarHeight - 24) / 2);
    }
    status_net_label_ = label(status_parent, last_status_net_.c_str(), &FONT_BODY,
                              dictation_.wifi_connected() ? (home ? theme::HomeInk : CYAN)
                                                         : (home ? theme::HomeMuted : MUTED));
    lv_obj_set_width(status_net_label_, 220);
    lv_obj_set_style_text_align(status_net_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(status_net_label_, width - 560, (kStatusBarHeight - 24) / 2);
    lv_obj_t* network_touch = lv_obj_create(status_parent);
    place(network_touch, width - 560, 0, 220, kStatusBarHeight);
    lv_obj_set_style_bg_opa(network_touch, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(network_touch, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(network_touch, 0, LV_PART_MAIN);
    lv_obj_remove_flag(network_touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(network_touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_touch, action_callback, LV_EVENT_CLICKED, make_action(ActionCode::OpenConnectivity));
    status_power_label_ = label(status_parent, last_status_power_.c_str(), &FONT_BODY,
                                home ? theme::HomeMuted : MUTED);
    lv_obj_set_width(status_power_label_, 320);
    lv_obj_set_style_text_align(status_power_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(status_power_label_, width - 340, (kStatusBarHeight - 24) / 2);
    if (home) {
        for (lv_obj_t* text : {status_net_label_, status_power_label_}) {
            lv_obj_set_style_bg_color(text, color(theme::HomeBackground), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(text, LV_OPA_90, LV_PART_MAIN);
        }
    }
}

void AppUi::render_home()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    const bool portrait = is_portrait();

    if (!home_scene_initialized_) {
        home_scene_.reset(static_cast<float>(width), static_cast<float>(height), esp_random());
        home_scene_initialized_ = true;
    } else if (home_scene_.width() != width || home_scene_.height() != height ||
               home_scene_rotation_ != current_rotation_) {
        home_scene_.resize(static_cast<float>(width), static_cast<float>(height),
                           shortest_rotation_turns(home_scene_rotation_, current_rotation_));
    }
    home_scene_rotation_ = current_rotation_;

    home_sandbox_ = lv_image_create(screen_);
    place(home_sandbox_, 0, 0, width, height);
    lv_obj_remove_flag(home_sandbox_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(home_sandbox_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(home_sandbox_, home_touch_callback, LV_EVENT_CLICKED, this);
    home_renderer_ = std::make_unique<HomeRenderer>();
    if (!home_renderer_->init(home_sandbox_, width, height)) {
        ESP_LOGW(TAG_UI, "Sin PSRAM para las partículas; Inicio sigue disponible");
        home_renderer_.reset();
        lv_obj_delete(home_sandbox_);
        home_sandbox_ = nullptr;
    } else {
        home_renderer_->draw(home_scene_);
    }

    const auto group = [](lv_obj_t* parent, int x, int y, int group_width, int group_height) {
        lv_obj_t* object = lv_obj_create(parent);
        place(object, x, y, group_width, group_height);
        lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(object, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
        return object;
    };

    const int header_width = portrait ? width - 48 : 260;
    home_header_group_ = group(screen_, 24, kStatusBarHeight + 10, header_width, 76);
    lv_obj_t* eyebrow = label(home_header_group_, "EL SISTEMA", &FONT_LABEL, theme::HomeMuted);
    lv_obj_set_pos(eyebrow, 0, 0);
    const std::string clock_text = status_clock_text();
    const size_t separator = clock_text.find(' ');
    const std::string time_text = separator == std::string::npos ? clock_text : clock_text.substr(0, separator);
    const std::string date_text = separator == std::string::npos ? "" : clock_text.substr(separator + 1);
    home_time_label_ = label(home_header_group_, time_text.c_str(), &FONT_NUMBER, theme::HomeInk);
    lv_obj_set_pos(home_time_label_, 0, 22);
    home_date_label_ = label(home_header_group_, date_text.c_str(), &FONT_LABEL, theme::HomeMuted);
    lv_obj_set_pos(home_date_label_, 132, 36);
    for (lv_obj_t* text : {eyebrow, home_time_label_, home_date_label_}) {
        lv_obj_set_style_bg_color(text, color(theme::HomeBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(text, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_pad_hor(text, 4, LV_PART_MAIN);
        lv_obj_remove_flag(text, LV_OBJ_FLAG_CLICKABLE);
    }

    constexpr int module_cell_width = 152;
    constexpr int module_cell_height = 110;
    const int module_columns = std::min<int>(portrait ? 3 : 5, kModules.size());
    const int module_rows = (static_cast<int>(kModules.size()) + module_columns - 1) / module_columns;
    const int module_group_width = module_cell_width * module_columns + 12 * (module_columns - 1);
    const int module_group_height = module_rows * module_cell_height + 12 * (module_rows - 1);
    home_modules_group_ = group(screen_, portrait ? (width - module_group_width) / 2 : width - module_group_width - 30,
                                height - module_group_height - (portrait ? 28 : 24),
                                module_group_width, module_group_height);
    for (size_t index = 0; index < kModules.size(); ++index) {
        const ModuleDescriptor& module = kModules[index];
        const int row = static_cast<int>(index) / module_columns;
        const int row_count = std::min<int>(module_columns, kModules.size() - row * module_columns);
        const int x = (module_columns - row_count) * (module_cell_width + 12) / 2 +
                      (static_cast<int>(index) % module_columns) * (module_cell_width + 12);
        const uint32_t accent = module.id == ModuleId::Productivity ? theme::HomeBlue :
                                module.id == ModuleId::Hourglass ? theme::HourglassGold :
                                module.id == ModuleId::Radar ? 0x21D783 :
                                module.id == ModuleId::Tetris ? 0xB979FF :
                                module.id == ModuleId::Maze ? 0xE4AA60 : theme::HomeCanary;

        lv_obj_t* cell = lv_button_create(home_modules_group_);
        place(cell, x, row * (module_cell_height + 12), module_cell_width, module_cell_height);
        lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cell, 0, LV_PART_MAIN);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(cell, action_callback, LV_EVENT_CLICKED,
                           make_action(ActionCode::OpenModule, 0, static_cast<int>(module.id)));

        constexpr int icon_size = 58;
        lv_obj_t* icon = lv_obj_create(cell);
        place(icon, (module_cell_width - icon_size) / 2, 0, icon_size, icon_size);
        lv_obj_set_style_bg_color(icon, color(accent), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(icon, color(theme::HomeIconInk), LV_PART_MAIN);
        lv_obj_set_style_border_width(icon, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(icon, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(icon, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(icon, color(theme::HomeIconInk), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(icon, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(icon, 1, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_x(icon, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_ofs_y(icon, 4, LV_PART_MAIN);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        const auto shape = [&](int sx, int sy, int sw, int sh, bool filled, int radius) {
            lv_obj_t* object = lv_obj_create(icon);
            place(object, sx, sy, sw, sh);
            lv_obj_set_style_bg_color(object, color(theme::HomeIconInk), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(object, filled ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_color(object, color(theme::HomeIconInk), LV_PART_MAIN);
            lv_obj_set_style_border_width(object, filled ? 0 : 3, LV_PART_MAIN);
            lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
            lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
            lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
        };
        if (module.id == ModuleId::Productivity) {
            // A compact checklist glyph, no font/icon asset dependency.
            shape(15, 11, 28, 38, false, 3);
            for (int line = 0; line < 3; ++line) {
                shape(20, 18 + line * 9, 4, 4, true, 1);
                shape(28, 19 + line * 9, 9, 2, true, 0);
            }
        } else if (module.id == ModuleId::Assistant) {
            // Friendly robot face; no StackChan servo/character resources.
            shape(12, 17, 34, 27, false, 7);
            shape(20, 26, 4, 6, true, 2);
            shape(34, 26, 4, 6, true, 2);
            shape(23, 37, 12, 2, true, 1);
            shape(28, 9, 2, 8, true, 0);
            shape(25, 5, 8, 8, true, LV_RADIUS_CIRCLE);
        } else if (module.id == ModuleId::Tetris) {
            shape(14, 14, 9, 9, true, 0);
            shape(24, 14, 9, 9, true, 0);
            shape(34, 14, 9, 9, true, 0);
            shape(24, 24, 9, 9, true, 0);
            shape(14, 36, 9, 9, true, 0);
            shape(24, 36, 9, 9, true, 0);
            shape(34, 36, 9, 9, true, 0);
        } else if (module.id == ModuleId::Maze) {
            shape(10,10,38,38,false,2);
            shape(20,11,3,24,true,0);
            shape(32,24,15,3,true,0);
            shape(23,34,14,3,true,0);
            shape(38,38,7,7,true,LV_RADIUS_CIRCLE);
        } else if (module.id == ModuleId::Radar) {
            shape(9, 9, 40, 40, false, LV_RADIUS_CIRCLE);
            shape(19, 19, 20, 20, false, LV_RADIUS_CIRCLE);
            shape(28, 10, 2, 38, true, 0);
            shape(10, 28, 38, 2, true, 0);
            shape(37, 16, 6, 6, true, LV_RADIUS_CIRCLE);
        } else {
            shape(13, 9, 32, 3, true, 0);
            shape(13, 46, 32, 3, true, 0);
            for (int row = 0; row < 34; ++row) {
                const int half = 3 + std::abs(row - 17) * 11 / 17;
                shape(29 - half, 12 + row, 2, 1, true, 0);
                shape(29 + half - 1, 12 + row, 2, 1, true, 0);
            }
            shape(26, 20, 6, 7, true, 1);
            shape(24, 39, 10, 5, true, 1);
        }
        lv_obj_t* name = label(cell, module.name, &FONT_LABEL, theme::HomeInk);
        lv_obj_set_width(name, module_cell_width);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(name, color(theme::HomeBackground), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(name, LV_OPA_90, LV_PART_MAIN);
        lv_obj_set_pos(name, 0, icon_size + 8);
        lv_obj_remove_flag(name, LV_OBJ_FLAG_CLICKABLE);
    }

    if (home_renderer_ != nullptr) {
        home_last_frame_us_ = home_fps_since_us_ = esp_timer_get_time();
        home_frame_count_ = 0;
        home_animation_timer_ = lv_timer_create(home_animation_callback, HOME_FRAME_MS, this);
    }
}

void AppUi::update_home_animation()
{
    if (view_ != View::Home || screen_off_ || home_renderer_ == nullptr) return;
    const int64_t now = esp_timer_get_time();
    const float dt = static_cast<float>(now - home_last_frame_us_) / 1000000.0f;
    home_last_frame_us_ = now;
    home_scene_.step(dt, orientation_.motion_snapshot(), current_rotation_);
    home_renderer_->draw(home_scene_);
    ++home_frame_count_;
    if (now - home_fps_since_us_ >= 10000000) {
        ESP_LOGI(TAG_UI, "Inicio: %.1f fotogramas/s, %u/%u partículas, sacudidas %lu",
                 home_frame_count_ * 1000000.0 / (now - home_fps_since_us_),
                 static_cast<unsigned>(home_scene_.active_count()),
                 static_cast<unsigned>(home_scene_.count()),
                 static_cast<unsigned long>(home_scene_.shake_count()));
        home_fps_since_us_ = now;
        home_frame_count_ = 0;
    }
}

void AppUi::render_radar()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const auto layout = radar_layout(width, lv_display_get_vertical_resolution(display_));
    const bool portrait = layout.portrait;
    const int right_x = layout.controls_x;
    const int right_w = layout.controls_width;
    auto* title = label(screen_, "RADAR", &FONT_DISPLAY, 0x7AF5AC);
    lv_obj_set_pos(title, right_x, layout.title_y);
    button(screen_, "INICIO", width-160, layout.title_y, 136, 48, ActionCode::GoHome);
    const char* filters[] = {"TODOS", "WI-FI", "BLE"};
    const int filter_width = portrait ? 116 : (right_w-16)/3;
    for (int i = 0; i < 3; ++i)
        radar_filter_buttons_[i] = button(screen_, filters[i], right_x+i*(filter_width+8), layout.filters_y, filter_width, 48, ActionCode::RadarFilter, 0, i);
    const int action_width = portrait ? 136 : (right_w-8)/2;
    const int pause_x = portrait ? 412 : right_x;
    radar_pause_button_ = button(screen_, "PAUSAR", pause_x, layout.actions_y, action_width, 48, ActionCode::RadarPause);
    radar_sound_button_ = button(screen_, "SONIDO: NO", pause_x+action_width+(portrait ? 12 : 8), layout.actions_y, action_width, 48, ActionCode::RadarSound);
    radar_status_ = label(screen_, "Preparando escaneo…", &FONT_BODY, MUTED);
    place(radar_status_, right_x, layout.status_y, right_w, portrait ? 28 : 48);
    lv_label_set_long_mode(radar_status_, LV_LABEL_LONG_DOT);
    radar_size_ = layout.diameter;
    auto* image = lv_image_create(screen_);
    lv_obj_set_pos(image, 0, 0);
    lv_obj_add_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(image, radar_touch_callback, LV_EVENT_CLICKED, this);
    radar_renderer_ = std::make_unique<RadarRenderer>();
    if (!radar_renderer_->init(image, radar_size_)) {
        radar_renderer_.reset();
        auto* failed = label(screen_, "Sin memoria para el radar. La lista sigue disponible.", &FONT_BODY, 0xFFD858);
        place(failed, 40, radar_size_/2-50, radar_size_-80, 100);
    }
    auto* legend = label(screen_, "INTENSIDAD · NO UBICACIÓN\nAmarillo: Wi-Fi  /  Diamante cian: BLE", &FONT_BODY, 0xD9FCE4);
    place(legend, right_x, layout.legend_y, right_w, 48);
    auto* details = panel(screen_, right_x, layout.detail_y, right_w, layout.detail_height, true);
    radar_detail_ = label(details, "Selecciona un punto o una fila para seguir su señal.", &FONT_BODY, TEXT);
    place(radar_detail_, 12, 10, right_w-242, LV_SIZE_CONTENT);
    radar_chart_ = lv_chart_create(details);
    place(radar_chart_, right_w-220, 12, 204, 120);
    base_style(radar_chart_, 0x071C12, 0x236841);
    lv_chart_set_type(radar_chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_range(radar_chart_, LV_CHART_AXIS_PRIMARY_Y, -110, -20);
    lv_chart_set_point_count(radar_chart_, 61);
    lv_chart_set_div_line_count(radar_chart_, 4, 4);
    radar_series_ = lv_chart_add_series(radar_chart_, color(0xFFD858), LV_CHART_AXIS_PRIMARY_Y);
    auto* graph_caption = label(details, "Últimos 60 s · RSSI", &FONT_BODY, MUTED);
    lv_obj_set_pos(graph_caption, right_w-218, 142);
    auto* list = panel(screen_, right_x, layout.list_y, right_w, layout.list_height, true);
    for (size_t i = 0; i < radar_rows_.size(); ++i) {
        auto* row = lv_button_create(list);
        place(row, 6, 6+static_cast<int>(i)*60, right_w-24, 54);
        base_style(row, 0x0B241A, 0x23513A);
        radar_row_actions_[i] = make_action(ActionCode::RadarSelect);
        lv_obj_add_event_cb(row, action_callback, LV_EVENT_CLICKED, radar_row_actions_[i]);
        auto* caption = label(row, "", &FONT_LABEL, TEXT);
        lv_obj_set_width(caption, right_w-44); lv_obj_center(caption);
        lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        radar_rows_[i] = row;
    }
    const int pager_y = layout.pager_y;
    button(screen_, "ANTERIOR", right_x, pager_y, 138, 48, ActionCode::RadarPage, 0, -1);
    button(screen_, "SIGUIENTE", right_x+right_w-138, pager_y, 138, 48, ActionCode::RadarPage, 0, 1);
    radar_page_label_ = label(screen_, "Buscando emisores…", &FONT_BODY, MUTED);
    place(radar_page_label_, right_x+146, pager_y+5, right_w-292, 42);
    lv_obj_set_style_text_align(radar_page_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    radar_next_update_ms_ = 0;
    radar_fps_since_ms_ = esp_timer_get_time()/1000; radar_frames_ = 0;
    update_radar();
    radar_animation_timer_ = lv_timer_create(radar_animation_callback, 30, this);
}

#ifdef EL_SISTEMA_RADAR_SMOKE
void AppUi::radar_validation_view(bool open, int rotation)
{
    // Test-only caller holds the BSP display lock, exactly like LVGL events.
    radar_validation_active_ = open;
    if (open) { open_module(ModuleId::Radar); set_rotation(rotation); }
    else { radar_.visible(false); radar_visible_ = false; overlay_ = Overlay::None; view_ = View::Home; }
    schedule_render();
}
#endif

void AppUi::radar_animation_callback(lv_timer_t* timer)
{ static_cast<AppUi*>(lv_timer_get_user_data(timer))->update_radar(); }

void AppUi::radar_touch_callback(lv_event_t* event)
{
    auto* ui = static_cast<AppUi*>(lv_event_get_user_data(event));
    auto* input = lv_indev_active();
    if (!input || ui->overlay_ != Overlay::None) return;
    lv_point_t point; lv_indev_get_point(input, &point);
    lv_area_t area; lv_obj_get_coords(static_cast<lv_obj_t*>(lv_event_get_target(event)), &area);
    const float radius = ui->radar_size_/2.f-22;
    ui->radar_hits_ = radar_hit_test(ui->radar_points_, (point.x-area.x1-ui->radar_size_/2.f)/radius,
                                    (point.y-area.y1-ui->radar_size_/2.f)/radius, 24.f/radius);
    if (ui->radar_hits_.size() == 1) {
        ui->radar_.select(ui->radar_hits_.front()); ui->radar_next_update_ms_ = 0;
    } else if (!ui->radar_hits_.empty()) {
        ui->overlay_ = Overlay::RadarPick; ui->schedule_render();
    }
}

void AppUi::update_radar()
{
    if (view_ != View::Radar || screen_off_) return;
    const int64_t now = esp_timer_get_time()/1000;
    const auto set_text = [](lv_obj_t* object, const std::string& text) {
        if (object && text != lv_label_get_text(object)) lv_label_set_text(object, text.c_str());
    };
    if (now >= radar_next_update_ms_) {
        radar_snapshot_ = radar_.snapshot();
        radar_next_update_ms_ = now+250;
        radar_points_ = radar_points(radar_snapshot_, now);
        for (size_t i = 0; i < radar_filter_buttons_.size(); ++i)
            lv_obj_set_style_border_color(radar_filter_buttons_[i], color(i == static_cast<size_t>(radar_snapshot_.filter) ? 0x7AF5AC : LINE), LV_PART_MAIN);
        std::string status = radar_snapshot_.paused ? "ESCANEO PAUSADO" :
            truncate(radar_snapshot_.wifi_status, 38)+" · "+truncate(radar_snapshot_.ble_status, 35);
        if (radar_snapshot_.limited) status += " · LISTA LIMITADA";
        set_text(radar_status_, status);
        set_text(lv_obj_get_child(radar_pause_button_, 0), radar_snapshot_.paused ? "REANUDAR" : "PAUSAR");
        set_text(lv_obj_get_child(radar_sound_button_, 0), radar_snapshot_.sound ? "SONIDO: SÍ" : "SONIDO: NO");
        std::vector<const RadarEmitter*> entries;
        const RadarEmitter* selected = nullptr;
        for (const auto& e : radar_snapshot_.emitters) {
            if (radar_matches(e.observation.kind, radar_snapshot_.filter)) entries.push_back(&e);
            if (e.id == radar_snapshot_.selected_id) selected = &e;
        }
        const int pages = std::max(1, static_cast<int>((entries.size()+11)/12));
        radar_page_ = std::clamp(radar_page_, 0, pages-1);
        for (size_t i = 0; i < radar_rows_.size(); ++i) {
            const size_t index = radar_page_*12+i;
            if (index >= entries.size()) { lv_obj_add_flag(radar_rows_[i], LV_OBJ_FLAG_HIDDEN); continue; }
            const auto& e = *entries[index];
            radar_row_actions_[i]->id = e.id;
            const std::string caption = std::string(e.observation.kind == RadarKind::Wifi ? "W " : "B ")+
                truncate(e.observation.name, 23)+" · "+std::to_string(e.observation.rssi)+" dBm"+
                (now-e.last_seen_ms >= 15000 ? " · Sin señal reciente" : "");
            set_text(lv_obj_get_child(radar_rows_[i],0), caption);
            lv_obj_remove_flag(radar_rows_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_border_color(radar_rows_[i], color(e.id == radar_snapshot_.selected_id ? 0xFFDB65 : 0x23513A), LV_PART_MAIN);
        }
        set_text(radar_page_label_, std::to_string(entries.size())+" emisores · "+std::to_string(radar_page_+1)+"/"+std::to_string(pages));
        int32_t values[61]; std::fill_n(values,61,LV_CHART_POINT_NONE);
        if (selected) {
            const auto& o = selected->observation;
            const int64_t age = std::max<int64_t>(0,now-selected->last_seen_ms)/1000;
            std::string detail = truncate(o.name,28)+"\n"+(o.kind == RadarKind::Wifi ? "WI-FI 2,4 GHz" : "BLUETOOTH LE")+
                " · "+radar_address(o)+"\n"+std::to_string(o.rssi)+" dBm · media "+
                std::to_string(static_cast<int>(std::round(selected->smoothed_rssi)))+" dBm\n";
            if (o.kind == RadarKind::Wifi) detail += "Canal "+std::to_string(o.channel)+(o.security ? " · Protegida\n" : " · Abierta\n");
            else detail += "Tipo dirección: "+std::to_string(o.address_type)+"\n";
            detail += age >= 15 ? "Sin señal reciente" : radar_trend_name(selected->trend);
            detail += " · Visto hace "+std::to_string(age)+" s";
            set_text(radar_detail_,detail);
            for (size_t i = 0; i < selected->history_count; ++i) {
                const auto& sample = selected->history[i];
                const int64_t seconds = (now-sample.time_ms)/1000;
                if (seconds >= 0 && seconds <= 60) values[60-seconds] = static_cast<int32_t>(std::round(sample.rssi));
            }
        } else set_text(radar_detail_, "Selecciona un punto o una fila para seguir su señal.\n\nLa intensidad depende también de paredes, antenas e interferencias.");
        auto* graph_values = lv_chart_get_y_array(radar_chart_,radar_series_);
        if (std::memcmp(graph_values, values, sizeof(values))) {
            std::copy_n(values,61,graph_values); lv_chart_refresh(radar_chart_);
        }
    }
    if (radar_renderer_ && overlay_ == Overlay::None) {
        radar_renderer_->draw(radar_points_,radar_snapshot_.selected_id, static_cast<float>(now%6000)*6.2831853f/6000.f);
        ++radar_frames_;
    }
    if (now-radar_fps_since_ms_ >= 10000) {
        ESP_LOGI(TAG_UI,"Radar %.1f fps de composición · %dx%d · %u emisores · %lu reportes descartados",
                 radar_frames_*1000.f/(now-radar_fps_since_ms_),
                 static_cast<int>(lv_display_get_horizontal_resolution(display_)),
                 static_cast<int>(lv_display_get_vertical_resolution(display_)),
                 static_cast<unsigned>(radar_snapshot_.emitters.size()),
                 static_cast<unsigned long>(radar_snapshot_.dropped_reports));
        radar_fps_since_ms_ = now; radar_frames_ = 0;
    }
}

void AppUi::render_hourglass_setup()
{
    lv_obj_t* title = label(screen_, "RELOJ DE ARENA", &FONT_NUMBER, theme::HourglassGold);
    lv_obj_set_pos(title, 32, 86);
    button(screen_, "INICIO", 544, 80, 144, 52, ActionCode::GoHome);
    lv_obj_t* help = label(screen_, "Elige cuánto tarda la arena en caer con el reloj vertical.", &FONT_BODY, TEXT);
    lv_obj_set_pos(help, 32, 150);
    lv_obj_set_width(help, 656);
    constexpr int presets[] = {1, 5, 10, 15, 25, 45, 60};
    for (size_t i = 0; i < std::size(presets); ++i) {
        const std::string text = std::to_string(presets[i]) + "m";
        button(screen_, text.c_str(), 32 + static_cast<int>(i) * 94, 220, 86, 60,
               ActionCode::HourglassPreset, 0, presets[i], nullptr, PANEL_ALT, theme::HourglassFrame);
    }
    lv_obj_t* caption = label(screen_, "DURACIÓN PERSONALIZADA · MINUTOS", &FONT_LABEL, theme::HomeMuted);
    lv_obj_set_pos(caption, 32, 330);
    std::string options;
    for (int minutes = 1; minutes <= 120; ++minutes) {
        if (minutes > 1) options += '\n';
        options += std::to_string(minutes);
    }
    hourglass_duration_roller_ = lv_roller_create(screen_);
    place(hourglass_duration_roller_, 230, 388, 260, 268);
    lv_roller_set_options(hourglass_duration_roller_, options.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(hourglass_duration_roller_, 5);
    lv_obj_set_style_text_font(hourglass_duration_roller_, &FONT_NUMBER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(hourglass_duration_roller_, 12, LV_PART_MAIN);
    lv_obj_set_style_text_color(hourglass_duration_roller_, color(theme::HomeMuted), LV_PART_MAIN);
    lv_obj_set_style_bg_color(hourglass_duration_roller_, color(PANEL_ALT), LV_PART_MAIN);
    lv_obj_set_style_border_color(hourglass_duration_roller_, color(theme::HourglassFrame), LV_PART_MAIN);
    lv_obj_set_style_bg_color(hourglass_duration_roller_, color(PANEL_ALT), LV_PART_SELECTED);
    lv_obj_set_style_text_color(hourglass_duration_roller_, color(theme::HourglassGold), LV_PART_SELECTED);
    lv_obj_set_style_text_font(hourglass_duration_roller_, &FONT_NUMBER, LV_PART_SELECTED);
    lv_roller_set_selected(hourglass_duration_roller_, hourglass_selected_minutes_ - 1, LV_ANIM_OFF);
    lv_obj_t* note = label(screen_,
        "Coloca la Tab5 vertical para iniciar. Inclinarla modifica el flujo; darle la vuelta hace caer la arena "
        "en sentido contrario.\n\nDurante el uso, toca la pantalla para mostrar los controles.", &FONT_BODY, MUTED);
    lv_obj_set_pos(note, 70, 730);
    lv_obj_set_width(note, 580);
    hourglass_error_label_ = label(screen_, "", &FONT_BODY, DANGER);
    lv_obj_set_pos(hourglass_error_label_, 70, 930);
    lv_obj_set_width(hourglass_error_label_, 580);
    button(screen_, hourglass_config_from_session_ ? "APLICAR E INICIAR" : "INICIAR", 70, 1020, 580, 72,
           ActionCode::HourglassStart, 0, 0, nullptr, PANEL_ALT, theme::HourglassGold);
    button(screen_, "CANCELAR", 70, 1110, 580, 60, ActionCode::HourglassCancelSetup);
}

void AppUi::render_hourglass()
{
    // Entire display is a reveal target; controls sit above this layer.
    lv_obj_t* touch = lv_obj_create(screen_);
    place(touch, 0, 0, 720, 1280);
    lv_obj_set_style_bg_opa(touch, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(touch, 0, LV_PART_MAIN);
    lv_obj_remove_flag(touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch, action_callback, LV_EVENT_CLICKED,
                        make_action(ActionCode::HourglassToggleControls));
    lv_obj_t* image = lv_image_create(screen_);
    place(image, 120, 60, 480, 1040);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_SCROLLABLE);
    hourglass_renderer_ = std::make_unique<HourglassRenderer>();
    if (!hourglass_renderer_->init(image)) {
        lv_obj_delete(image);
        hourglass_renderer_.reset();
        hourglass_.pause();
        hourglass_controls_open_ = true;
        show_info("Sin memoria para el reloj", "Vuelve a Inicio y abre el módulo de nuevo.");
    }
    hourglass_time_label_ = label(screen_, "00:00", &FONT_DISPLAY, theme::HomeInk);
    lv_obj_set_width(hourglass_time_label_, 600);
    lv_obj_set_style_text_align(hourglass_time_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(hourglass_time_label_, 60, 1170);
    lv_obj_remove_flag(hourglass_time_label_, LV_OBJ_FLAG_CLICKABLE);

    hourglass_controls_ = panel(screen_, 32, 944, 656, 300);
    lv_obj_set_style_bg_opa(hourglass_controls_, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(hourglass_controls_, color(theme::HourglassFrame), LV_PART_MAIN);
    hourglass_error_label_ = label(hourglass_controls_, "", &FONT_BODY, DANGER);
    lv_obj_set_pos(hourglass_error_label_, 16, 10);
    lv_obj_set_width(hourglass_error_label_, 624);
    hourglass_primary_ = button(hourglass_controls_, "PAUSAR", 16, 70, 300, 56, ActionCode::HourglassPauseResume,
                                0, 0, nullptr, PANEL_ALT, theme::HourglassGold);
    button(hourglass_controls_, "REINICIAR", 336, 70, 300, 56, ActionCode::HourglassAskReset);
    button(hourglass_controls_, "DURACIÓN", 16, 140, 300, 56, ActionCode::HourglassDuration);
    button(hourglass_controls_, "INICIO", 336, 140, 300, 56, ActionCode::GoHome);
    button(hourglass_controls_, "OCULTAR CONTROLES", 16, 222, 620, 56, ActionCode::HourglassToggleControls);
    if (!hourglass_controls_open_) lv_obj_add_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
    hourglass_controls_since_us_ = esp_timer_get_time();
    hourglass_fps_since_us_ = esp_timer_get_time();
    hourglass_frames_ = 0;
    update_hourglass();
}

void AppUi::update_hourglass()
{
    if (screen_off_ || (view_ != View::Hourglass && view_ != View::HourglassSetup)) return;
    const HourglassSnapshot snapshot = hourglass_.snapshot();
    if (hourglass_start_pending_ && snapshot.command_id >= hourglass_pending_command_) {
        if (snapshot.state == HourglassState::Running) {
            hourglass_start_pending_ = false;
            hourglass_controls_open_ = false;
            view_ = View::Hourglass;
            schedule_render();
            return;
        }
        if (snapshot.error != HourglassError::None) hourglass_start_pending_ = false;
    }
    const std::string error = hourglass_error_text(snapshot.error);
    if (hourglass_error_label_ && error != hourglass_last_error_) {
        lv_label_set_text(hourglass_error_label_, error.c_str());
        hourglass_last_error_ = error;
        if (!error.empty() && hourglass_controls_) {
            hourglass_controls_open_ = true;
            lv_obj_remove_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (view_ == View::HourglassSetup) return;
    const uint64_t seconds = snapshot.elapsed_us / 1000000;
    char text[64];
    if (seconds >= 3600) {
        std::snprintf(text, sizeof(text), "%llu:%02llu:%02llu", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    } else {
        std::snprintf(text, sizeof(text), "%02llu:%02llu", seconds / 60, seconds % 60);
    }
    std::string time = text;
    if (snapshot.state == HourglassState::Completed) time += " · FINALIZADO";
    else if (snapshot.state == HourglassState::Paused) time += " · PAUSADO";
    if (hourglass_time_label_ && time != hourglass_last_time_) {
        lv_label_set_text(hourglass_time_label_, time.c_str());
        hourglass_last_time_ = time;
    }
    if (hourglass_primary_) {
        const char* caption = snapshot.state == HourglassState::Running ? "PAUSAR" :
                              snapshot.state == HourglassState::Paused ? "REANUDAR" :
                              snapshot.state == HourglassState::Ready ? "INICIAR" : "CONTINUAR";
        lv_obj_t* label = lv_obj_get_child(hourglass_primary_, 0);
        if (std::strcmp(lv_label_get_text(label), caption) != 0) lv_label_set_text(label, caption);
    }
    const int64_t now = esp_timer_get_time();
    if (hourglass_controls_open_ && snapshot.state == HourglassState::Running && error.empty() &&
        overlay_ == Overlay::None && now - hourglass_controls_since_us_ >= 4000000) {
        hourglass_controls_open_ = false;
        if (hourglass_controls_) lv_obj_add_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
    }
    if (hourglass_renderer_ && (snapshot.frame != hourglass_last_frame_ ||
                               snapshot.generation != hourglass_last_generation_)) {
        hourglass_renderer_->draw(snapshot);
        hourglass_last_frame_ = snapshot.frame;
        hourglass_last_generation_ = snapshot.generation;
        ++hourglass_frames_;
    }
    if (now - hourglass_fps_since_us_ >= 10000000) {
        ESP_LOGI(TAG_UI, "Reloj de arena: %.1f FPS, masa superior %.6f, inferior %.6f, estado %u",
                 hourglass_frames_ * 1000000.0 / (now - hourglass_fps_since_us_),
                 snapshot.upper_mass, snapshot.lower_mass, static_cast<unsigned>(snapshot.state));
        hourglass_fps_since_us_ = now;
        hourglass_frames_ = 0;
    }
}

void AppUi::leave_hourglass(View target)
{
    hourglass_.set_visible(false);
    hourglass_visible_ = false;
    hourglass_start_pending_ = false;
    view_ = target;
    const int detected = orientation_.poll();
    current_rotation_ = detected >= 0 ? detected : hourglass_return_rotation_;
    orientation_.set_current(current_rotation_);
    static const lv_display_rotation_t rotations[] = {LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90,
                                                      LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270};
    lv_display_set_rotation(display_, rotations[current_rotation_]);
    home_rotation_pending_ = false;
    home_rotation_sources_.clear();
}

void AppUi::render_header(bool compact)
{
    const int width = lv_display_get_horizontal_resolution(display_);
    lv_obj_t* header = panel(screen_, 0, kStatusBarHeight, width, compact ? 78 : 90, false);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header, color(PANEL), LV_PART_MAIN);

    lv_obj_t* title = label(header, "EL SISTEMA", &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(title, 20, compact ? 14 : 20);
    lv_obj_t* subtitle = label(header, "MÓDULO DE PRODUCTIVIDAD", &FONT_LABEL, CYAN);
    lv_obj_set_pos(subtitle, 22, compact ? 45 : 57);

    if (!compact) {
        const uint32_t required = XpService::required_for_level(state_.profile.level);
        char profile[96];
        std::snprintf(profile, sizeof(profile), "NIVEL %lu   %lu / %lu XP",
                      static_cast<unsigned long>(state_.profile.level),
                      static_cast<unsigned long>(state_.profile.level_xp), static_cast<unsigned long>(required));
        lv_obj_t* xp = label(header, profile, &FONT_LABEL, CYAN);
        lv_obj_set_pos(xp, std::max(280, width / 2 - 130), 34);
    }

    if (repository_.storage_percent() >= 80) {
        char storage[48];
        std::snprintf(storage, sizeof(storage), "ALMACENAMIENTO %u%%", repository_.storage_percent());
        lv_obj_t* warning = label(header, storage, &FONT_LABEL, DANGER);
        lv_obj_set_pos(warning, width - 520, 8);
    }

    button(header, "INICIO", width - 430, 17, 126, 50, ActionCode::GoHome, 0, 0, nullptr,
           PANEL_ALT, VIOLET);
    button(header, "HISTORIAL", width - 294, 17, 126, 50, ActionCode::OpenHistory);
    button(header, "+ NUEVA MISIÓN", width - 158, 17, 144, 50, ActionCode::NewMission, 0, 0, nullptr, ACCENT_SOFT,
           CYAN);
}

void AppUi::render_dashboard()
{
    render_header(is_portrait());
    if (is_portrait()) render_portrait_dashboard();
    else render_landscape_dashboard();
}

void AppUi::render_assistant()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    const bool portrait = is_portrait();
    const int margin = portrait ? 14 : 18;
    const int gap = 14;
    const int header_y = kStatusBarHeight;
    const int header_height = 78;
    const int top = header_y + header_height + 12;
    const int usable_height = height - top - margin;

    lv_obj_t* header = panel(screen_, 0, header_y, width, header_height, false);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_t* title = label(header, "ASISTENTE", &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(title, 20, 10);
    const AssistantVoiceSnapshot voice = assistant_voice_.snapshot();
    lv_obj_t* subtitle = label(header, "BUCLE DE VOZ LOCAL · A2", &FONT_LABEL, VIOLET);
    lv_obj_set_pos(subtitle, 22, 43);
    lv_obj_t* scope = label(header, "SIN IA CONVERSACIONAL · SIN FUNCIONES SOCIALES", &FONT_BODY, MUTED);
    lv_obj_set_width(scope, 310);
    lv_obj_set_style_text_align(scope, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(scope, width - 470, 26);
    button(header, "INICIO", width - 150, 14, 132, 50, ActionCode::GoHome);

    const int stage_width = portrait ? width - margin * 2 : (width * 65) / 100;
    const int stage_height = portrait ? std::min(700, (usable_height * 63) / 100) : usable_height;
    lv_obj_t* stage = panel(screen_, margin, top, stage_width, stage_height, false);
    lv_obj_set_style_border_color(stage, color(VIOLET), LV_PART_MAIN);
    lv_obj_t* stage_caption = label(stage, "ENTIDAD VISUAL DEL SISTEMA", &FONT_LABEL, MUTED);
    lv_obj_set_pos(stage_caption, 18, 14);

    const int character_x = (stage_width - 320) / 2;
    assistant_character_base_y_ = portrait ? 62 : 34;

    assistant_aura_ = lv_obj_create(stage);
    place(assistant_aura_, character_x + 35, assistant_character_base_y_ + 12, 250, 250);
    lv_obj_set_style_bg_opa(assistant_aura_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(assistant_aura_, color(CYAN), LV_PART_MAIN);
    lv_obj_set_style_border_width(assistant_aura_, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(assistant_aura_, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_radius(assistant_aura_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_remove_flag(assistant_aura_, LV_OBJ_FLAG_SCROLLABLE);

    assistant_character_ = lv_obj_create(stage);
    place(assistant_character_, character_x, assistant_character_base_y_, 320, 320);
    lv_obj_set_style_bg_opa(assistant_character_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(assistant_character_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(assistant_character_, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(assistant_character_, 0, LV_PART_MAIN);
    lv_obj_remove_flag(assistant_character_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(assistant_character_, LV_OBJ_FLAG_CLICKABLE);
    const ActionCode character_action = voice.phase == AssistantVoicePhase::Listening
                                            ? ActionCode::AssistantVoiceStop
                                            : (assistant_voice_.busy() ? ActionCode::AssistantVoiceCancel
                                                                       : ActionCode::AssistantVoiceStart);
    lv_obj_add_event_cb(assistant_character_, action_callback, LV_EVENT_CLICKED, make_action(character_action));

    lv_obj_t* character_image = lv_image_create(assistant_character_);
    lv_image_set_src(character_image, &assistant_character);
    lv_obj_set_pos(character_image, 0, 0);
    lv_obj_remove_flag(character_image, LV_OBJ_FLAG_CLICKABLE);

    auto face_part = [&](int x, int y, int part_width, int part_height) {
        lv_obj_t* object = lv_obj_create(assistant_character_);
        place(object, x, y, part_width, part_height);
        lv_obj_set_style_bg_color(object, color(CYAN), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
        return object;
    };
    assistant_eye_left_ = face_part(124, 101, 20, 7);
    assistant_eye_right_ = face_part(177, 101, 20, 7);
    assistant_mouth_ = face_part(147, 132, 26, 4);
    assistant_core_ = face_part(146, 164, 28, 28);
    lv_obj_set_style_bg_opa(assistant_core_, LV_OPA_30, LV_PART_MAIN);

    const int state_y = assistant_character_base_y_ + 338;
    assistant_state_label_ = label(stage, "", &FONT_DISPLAY, CYAN);
    lv_obj_set_width(assistant_state_label_, stage_width - 36);
    lv_obj_set_style_text_align(assistant_state_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(assistant_state_label_, 18, state_y);
    assistant_message_label_ = label(stage, "", &FONT_BODY, MUTED);
    lv_obj_set_width(assistant_message_label_, stage_width - 56);
    lv_obj_set_style_text_align(assistant_message_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(assistant_message_label_, 28, state_y + 42);
    assistant_level_bar_ = lv_bar_create(stage);
    place(assistant_level_bar_, 54, state_y + 88, stage_width - 108, 12);
    lv_bar_set_range(assistant_level_bar_, 0, 100);
    lv_bar_set_value(assistant_level_bar_,
                     static_cast<int>(std::clamp(voice.level * 450.0f, 0.0f, 100.0f)), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(assistant_level_bar_, color(PANEL_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(assistant_level_bar_, color(CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_opa(assistant_level_bar_,
                         voice.phase == AssistantVoicePhase::Listening ? LV_OPA_COVER : LV_OPA_30,
                         LV_PART_MAIN);

    const int controls_x = portrait ? margin : margin + stage_width + gap;
    const int controls_y = portrait ? top + stage_height + gap : top;
    const int controls_width = portrait ? width - margin * 2 : width - controls_x - margin;
    const int controls_height = portrait ? height - controls_y - margin : usable_height;
    lv_obj_t* controls = panel(screen_, controls_x, controls_y, controls_width, controls_height, false);
    lv_obj_t* controls_title = label(controls, "TURNO DE VOZ", &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(controls_title, 20, 20);
    lv_obj_t* controls_body = label(
        controls,
        "El personaje escucha con los micrófonos, transcribe con Whisper en tu Mac y confirma por el altavoz. "
        "La respuesta todavía es local y determinista.",
        &FONT_BODY, MUTED);
    lv_obj_set_pos(controls_body, 20, 62);
    lv_obj_set_width(controls_body, controls_width - 40);

    const int button_width = portrait ? std::min(200, (controls_width - 60) / 3) : controls_width - 40;
    const bool listening = voice.phase == AssistantVoicePhase::Listening;
    const bool voice_busy = assistant_voice_.busy();
    const bool can_retry = voice.phase == AssistantVoicePhase::Error && voice.retry_available;
    const char* primary_text = listening ? "DETENER" : (can_retry ? "REINTENTAR" : "HABLAR");
    const ActionCode primary_action = listening ? ActionCode::AssistantVoiceStop
                                                : (can_retry ? ActionCode::AssistantVoiceRetry
                                                             : ActionCode::AssistantVoiceStart);
    if (portrait) {
        button(controls, primary_text, 20, 138, button_width, 54, primary_action, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
        button(controls, voice_busy ? "CANCELAR" : "CICLO VISUAL", 30 + button_width, 138, button_width, 54,
               voice_busy ? ActionCode::AssistantVoiceCancel : ActionCode::AssistantDemo);
        if (!voice_busy) {
            button(controls, "CELEBRAR", 40 + button_width * 2, 138, button_width, 54,
                   ActionCode::AssistantCelebrate, 0, 0, nullptr, PANEL_ALT, GOLD);
        }
    } else {
        button(controls, primary_text, 20, 150, button_width, 56, primary_action, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
        button(controls, voice_busy ? "CANCELAR TURNO" : "PROBAR CICLO VISUAL", 20, 220, button_width, 56,
               voice_busy ? ActionCode::AssistantVoiceCancel : ActionCode::AssistantDemo);
        if (!voice_busy) {
            button(controls, "PROBAR CELEBRACIÓN", 20, 290, button_width, 56, ActionCode::AssistantCelebrate, 0, 0,
                   nullptr, PANEL_ALT, GOLD);
        }
    }

    std::string turn_detail = "ALCANCE A2\n• Captura y transcripción reales\n• Respuesta hablada local\n• Estados no bloqueantes";
    if (!voice.transcript.empty()) turn_detail += "\n\nTÚ: “" + voice.transcript + "”";
    if (!voice.response.empty()) turn_detail += "\nSISTEMA: “" + voice.response + "”";
    if (!voice.error.empty()) turn_detail += "\n\n" + voice.error;
    lv_obj_t* boundary = label(controls, turn_detail.c_str(), &FONT_BODY,
                               voice.phase == AssistantVoicePhase::Error ? DANGER : TEXT);
    lv_obj_set_pos(boundary, 20, portrait ? 218 : 382);
    lv_obj_set_width(boundary, controls_width - 40);

    last_assistant_generation_ = assistant_.snapshot().generation;
    last_assistant_voice_generation_ = voice.generation;
    assistant_animation_tick_ = 0;
    refresh_assistant_visual();
}

void AppUi::refresh_assistant_visual()
{
    if (view_ != View::Assistant || assistant_character_ == nullptr) return;
    const AssistantSnapshot snapshot = assistant_.snapshot();
    const uint32_t phase = assistant_animation_tick_++;
    static constexpr int8_t kBob[] = {0, 0, -1, -2, -3, -3, -2, -1, 0, 0, 1, 2, 2, 1};
    lv_obj_set_y(assistant_character_, assistant_character_base_y_ + kBob[phase % (sizeof(kBob) / sizeof(kBob[0]))]);

    uint32_t face_color = CYAN;
    if (snapshot.state == AssistantVisualState::Thinking) face_color = VIOLET;
    if (snapshot.state == AssistantVisualState::Celebrating) face_color = GOLD;
    const lv_opa_t pulse = static_cast<lv_opa_t>(LV_OPA_20 + (phase % 8) * 5);
    lv_obj_set_style_border_color(assistant_aura_, color(face_color), LV_PART_MAIN);
    lv_obj_set_style_border_opa(assistant_aura_, pulse, LV_PART_MAIN);
    lv_obj_set_style_bg_color(assistant_core_, color(face_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(assistant_core_, static_cast<lv_opa_t>(LV_OPA_20 + (phase % 6) * 8), LV_PART_MAIN);

    auto set_part = [&](lv_obj_t* object, int x, int y, int part_width, int part_height, lv_opa_t opacity) {
        place(object, x, y, part_width, part_height);
        lv_obj_set_style_bg_color(object, color(face_color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(object, opacity, LV_PART_MAIN);
        lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    };
    const bool blink = snapshot.state == AssistantVisualState::Idle && phase % 20 == 0;
    switch (snapshot.state) {
        case AssistantVisualState::Dormant:
            set_part(assistant_eye_left_, 124, 103, 20, 3, LV_OPA_30);
            set_part(assistant_eye_right_, 177, 103, 20, 3, LV_OPA_30);
            set_part(assistant_mouth_, 151, 133, 18, 3, LV_OPA_20);
            break;
        case AssistantVisualState::Greeting:
            set_part(assistant_eye_left_, 128, 98, 14, 14, LV_OPA_COVER);
            set_part(assistant_eye_right_, 179, 98, 14, 14, LV_OPA_COVER);
            set_part(assistant_mouth_, 146, 131, 30, 5, LV_OPA_COVER);
            break;
        case AssistantVisualState::Idle:
            set_part(assistant_eye_left_, 122, blink ? 104 : 101, 22, blink ? 2 : 7, LV_OPA_COVER);
            set_part(assistant_eye_right_, 177, blink ? 104 : 101, 22, blink ? 2 : 7, LV_OPA_COVER);
            set_part(assistant_mouth_, 149, 132, 22, 4, LV_OPA_80);
            break;
        case AssistantVisualState::Attentive:
            set_part(assistant_eye_left_, 128, 98, 14, 14, LV_OPA_COVER);
            set_part(assistant_eye_right_, 179, 98, 14, 14, LV_OPA_COVER);
            set_part(assistant_mouth_, 151, 132, 18, 4, LV_OPA_80);
            break;
        case AssistantVisualState::Listening:
            set_part(assistant_eye_left_, 132, 95, 8, 22, LV_OPA_COVER);
            set_part(assistant_eye_right_, 181, 95, 8, 22, LV_OPA_COVER);
            set_part(assistant_mouth_, 154, 132, 12, 4, LV_OPA_60);
            break;
        case AssistantVisualState::Thinking:
            set_part(assistant_eye_left_, 120, 99, 23, 6, LV_OPA_COVER);
            set_part(assistant_eye_right_, 178, 107, 23, 6, LV_OPA_COVER);
            set_part(assistant_mouth_, 153, 133, 14, 3, LV_OPA_60);
            break;
        case AssistantVisualState::Speaking: {
            const int mouth_height = phase % 4 < 2 ? 5 : 12;
            set_part(assistant_eye_left_, 123, 101, 21, 7, LV_OPA_COVER);
            set_part(assistant_eye_right_, 177, 101, 21, 7, LV_OPA_COVER);
            set_part(assistant_mouth_, 145, 128, 30, mouth_height, LV_OPA_COVER);
            break;
        }
        case AssistantVisualState::Celebrating:
            set_part(assistant_eye_left_, 118, 102, 28, 5, LV_OPA_COVER);
            set_part(assistant_eye_right_, 175, 102, 28, 5, LV_OPA_COVER);
            set_part(assistant_mouth_, 140, 128, 40, 9, LV_OPA_COVER);
            break;
    }

    const AssistantVoiceSnapshot voice = assistant_voice_.snapshot();
    const bool voice_has_state = voice.phase != AssistantVoicePhase::Idle;
    lv_label_set_text(assistant_state_label_, voice_has_state ? assistant_voice_phase_name(voice.phase)
                                                             : assistant_state_name(snapshot.state));
    lv_obj_set_style_text_color(assistant_state_label_, color(face_color), LV_PART_MAIN);
    const std::string voice_message = !voice.error.empty() ? voice.error : voice.message;
    lv_label_set_text(assistant_message_label_, voice_has_state && !voice_message.empty()
                                                   ? voice_message.c_str()
                                                   : assistant_state_message(snapshot.state));
    if (assistant_level_bar_ != nullptr) {
        lv_bar_set_value(assistant_level_bar_,
                         static_cast<int>(std::clamp(voice.level * 450.0f, 0.0f, 100.0f)), LV_ANIM_OFF);
        lv_obj_set_style_opa(assistant_level_bar_,
                             voice.phase == AssistantVoicePhase::Listening ? LV_OPA_COVER : LV_OPA_30,
                             LV_PART_MAIN);
    }
}

void AppUi::render_landscape_dashboard()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    const int top = 102 + kStatusBarHeight;
    const int gap = 14;
    const int margin = 18;
    const int body_height = height - top - 16;

    // Franja de pestanas en el margen derecho. La lista se abre a su izquierda y se
    // colapsa al tocar la pestana activa, devolviendo todo el ancho a la mision
    // activa, que es el bloque protagonista.
    constexpr int kTabWidth = 90;
    constexpr int kTabHeight = 70;
    constexpr int kTabGap = 8;
    const int strip_x = width - margin - kTabWidth;

    // La geometria se calcula aqui en vez de preguntarsela a los widgets: en LVGL 9
    // lv_obj_get_width/height devuelven obj->coords, que solo se rellena en la pasada
    // de layout, asi que justo despues de crearlos valdria 0.
    const int content_right = strip_x - gap;
    const int available = content_right - margin;
    const bool list_open = side_panel_ >= 0;
    // El cajon se superpone temporalmente: abrir una lista no reduce ni recompone
    // la mision activa, que conserva siempre todo el espacio disponible.
    const int list_width = std::min(560, available);
    const int active_width = available;

    const Mission* mission = find_mission(state_, state_.focus.active_id);
    lv_obj_t* active = panel(screen_, margin, top, active_width, body_height, false);
    if (mission != nullptr) {
        // El marco toma el color del rango: la importancia se lee de un vistazo.
        lv_obj_set_style_border_color(active, color(rank_color(mission->rank)), LV_PART_MAIN);
        lv_obj_set_style_border_width(active, 2, LV_PART_MAIN);
    }
    render_active_panel(active, active_width, body_height);

    if (list_open) {
        lv_obj_t* list = panel(screen_, content_right - list_width, top, list_width, body_height, true);
        lv_obj_set_style_border_color(list, color(side_panel_ == 0 ? VIOLET : CYAN), LV_PART_MAIN);
        if (side_panel_ == 0) render_focus_list(list, list_width, body_height);
        else render_backlog_list(list, list_width, body_height);
    }

    // Las pestanas se crean al final para quedar siempre por encima del cajon.
    button(screen_, "ENFOQUE", strip_x, top, kTabWidth, kTabHeight, ActionCode::ToggleSidePanel, 0, 0, nullptr,
           side_panel_ == 0 ? ACCENT_SOFT : PANEL_ALT, side_panel_ == 0 ? CYAN : LINE);
    button(screen_, "BACKLOG", strip_x, top + kTabHeight + kTabGap, kTabWidth, kTabHeight,
           ActionCode::ToggleSidePanel, 0, 1, nullptr, side_panel_ == 1 ? ACCENT_SOFT : PANEL_ALT,
           side_panel_ == 1 ? CYAN : LINE);
}

void AppUi::render_portrait_dashboard()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    constexpr int kMargin = 14;
    constexpr int kTop = 90 + kStatusBarHeight;
    constexpr int kGap = 10;
    constexpr int kTabWidth = 82;
    constexpr int kTabHeight = 76;
    constexpr int kTabGap = 8;
    const int body_height = height - kTop - 16;
    const int strip_x = width - kMargin - kTabWidth;
    const int content_right = strip_x - kGap;
    const int active_width = content_right - kMargin;

    const Mission* mission = find_mission(state_, state_.focus.active_id);
    lv_obj_t* active = panel(screen_, kMargin, kTop, active_width, body_height, false);
    if (mission != nullptr) {
        lv_obj_set_style_border_color(active, color(rank_color(mission->rank)), LV_PART_MAIN);
        lv_obj_set_style_border_width(active, 2, LV_PART_MAIN);
    }
    render_active_panel(active, active_width, body_height);

    // En vertical el cajon ocupa el ancho de la mision, pero solo mientras se
    // consulta. Cerrado, desaparece por completo y deja el foco en primer plano.
    if (side_panel_ >= 0) {
        const int drawer_width = active_width;
        lv_obj_t* list = panel(screen_, content_right - drawer_width, kTop, drawer_width, body_height, true);
        lv_obj_set_style_border_color(list, color(side_panel_ == 0 ? VIOLET : CYAN), LV_PART_MAIN);
        if (side_panel_ == 0) render_focus_list(list, drawer_width, body_height);
        else render_backlog_list(list, drawer_width, body_height);
    }

    button(screen_, "ENFOQUE", strip_x, kTop, kTabWidth, kTabHeight, ActionCode::ToggleSidePanel, 0, 0, nullptr,
           side_panel_ == 0 ? ACCENT_SOFT : PANEL_ALT, side_panel_ == 0 ? CYAN : LINE);
    button(screen_, "BACKLOG", strip_x, kTop + kTabHeight + kTabGap, kTabWidth, kTabHeight,
           ActionCode::ToggleSidePanel, 0, 1, nullptr, side_panel_ == 1 ? ACCENT_SOFT : PANEL_ALT,
           side_panel_ == 1 ? CYAN : LINE);
}

void AppUi::render_active_panel(lv_obj_t* parent, int width, int height)
{
    const Mission* mission = find_mission(state_, state_.focus.active_id);
    if (mission == nullptr) {
        lv_obj_t* empty = label(parent, "NINGUNA MISIÓN EN CURSO", &FONT_DISPLAY, MUTED);
        lv_obj_set_width(empty, width - 230);
        lv_obj_set_pos(empty, 24, 22);
        lv_obj_t* help = label(parent, "Elige una misión de tu Zona de Enfoque para comenzar.", &FONT_BODY, TEXT);
        lv_obj_set_width(help, width - 230);
        lv_obj_set_pos(help, 26, 58);
        button(parent, "+ NUEVA MISIÓN", width - 194, 24, 170, 52, ActionCode::NewMission, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);

        // La pantalla principal nunca queda vacia: si no hay cronometro activo,
        // la cola de enfoque ocupa el bloque protagonista y permite iniciar la
        // siguiente mision sin abrir el cajon lateral.
        const int list_top = 102;
        lv_obj_t* focus = panel(parent, 18, list_top, width - 36, height - list_top - 18, true);
        lv_obj_set_style_border_color(focus, color(VIOLET), LV_PART_MAIN);
        render_focus_list(focus, width - 36, height - list_top - 18);
        return;
    }

    const uint32_t rank_tint = rank_color(mission->rank);

    // Franja superior con el color del rango: la importancia del bloque se lee antes
    // que cualquier texto.
    lv_obj_t* stripe = lv_obj_create(parent);
    place(stripe, 0, 0, width, 5);
    lv_obj_set_style_bg_color(stripe, color(rank_tint), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(stripe, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(stripe, 0, LV_PART_MAIN);
    lv_obj_remove_flag(stripe, LV_OBJ_FLAG_SCROLLABLE);

    const int badge_size = 96;

    // Distintivo de rango: la letra en grande sobre una placa tenida.
    lv_obj_t* badge = panel(parent, 24, 22, badge_size, badge_size, false);
    lv_obj_set_style_bg_color(badge, color(ACCENT_SOFT), LV_PART_MAIN);
    lv_obj_set_style_border_color(badge, color(rank_tint), LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 2, LV_PART_MAIN);
    lv_obj_t* letter = label(badge, rank_to_string(mission->rank), &FONT_NUMBER, rank_tint);
    lv_obj_center(letter);

    lv_obj_t* heading = label(parent, "MISIÓN ACTIVA", &FONT_LABEL, CYAN);
    const int meta_x = 24 + badge_size + 16;
    lv_obj_set_pos(heading, meta_x, 24);
    char rank_text[64];
    std::snprintf(rank_text, sizeof(rank_text), "RANGO %s · %s", rank_to_string(mission->rank),
                  rank_importance(mission->rank));
    lv_obj_t* rank_label = label(parent, rank_text, &FONT_LABEL, rank_tint);
    lv_obj_set_pos(rank_label, meta_x, 58);

    // Jerarquia deliberadamente vertical: metadatos, cronometro, titulo y
    // descripcion. Es la misma en ambas orientaciones para que la mision activa
    // mantenga siempre el protagonismo.
    constexpr int kTimerY = 126;
    timer_label_ = label(parent, duration_text(service_.current_elapsed(*mission)).c_str(), &FONT_HERO, CYAN);
    lv_obj_set_width(timer_label_, width - 48);
    lv_obj_set_style_text_align(timer_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(timer_label_, 24, kTimerY);
    active_status_label_ = label(parent, "CRONÓMETRO ACTIVO", &FONT_LABEL, CYAN);
    lv_obj_set_width(active_status_label_, width - 48);
    lv_obj_set_style_text_align(active_status_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    const int timer_bottom = kTimerY + lv_font_get_line_height(&FONT_HERO);
    lv_obj_set_pos(active_status_label_, 24, timer_bottom - 2);

    const int content_y = timer_bottom + 44;
    lv_obj_t* title = label(parent, mission->title.c_str(), &FONT_NUMBER, TEXT);
    lv_obj_set_width(title, width - 72);
    lv_obj_set_pos(title, 30, content_y);
    if (!mission->description.empty()) {
        lv_obj_t* description = label(parent, mission->description.c_str(), &FONT_BODY, MUTED);
        lv_obj_set_width(description, width - 72);
        lv_obj_set_pos(description, 32, content_y + 82);
    }

    const int y = height - 76;
    button(parent, "PAUSAR", 24, y, 120, 48, ActionCode::PauseMission, mission->id, 0, nullptr, PANEL_ALT, CYAN);
    button(parent, "EDITAR", 156, y, 120, 48, ActionCode::EditMission, mission->id);
    button(parent, "COMPLETAR", 288, y, 152, 48, ActionCode::CompleteMission, mission->id, 0, nullptr, ACCENT_SOFT,
           CYAN);
    button(parent, "MIC", width - 108, y, 84, 48, ActionCode::OpenDictation);
}

void AppUi::render_focus_list(lv_obj_t* parent, int width, int)
{
    lv_obj_t* heading = label(parent, "ZONA DE ENFOQUE", &FONT_LABEL, VIOLET);
    lv_obj_set_pos(heading, 18, 15);
    int y = 48;
    int count = 0;
    for (MissionId id : state_.focus.ordered_ids) {
        const Mission* mission = find_mission(state_, id);
        if (mission == nullptr || mission->status == MissionStatus::Completed || id == state_.focus.active_id) continue;
        lv_obj_t* row = panel(parent, 12, y, width - 36, is_portrait() ? 144 : 116, false);
        render_mission_row(row, *mission, true, width - 36);
        y += is_portrait() ? 154 : 126;
        ++count;
    }
    if (count == 0) {
        lv_obj_t* empty = label(parent, "La cola está vacía.", &FONT_BODY, MUTED);
        lv_obj_set_pos(empty, 18, 61);
    }
}

void AppUi::render_backlog_list(lv_obj_t* parent, int width, int)
{
    lv_obj_t* heading = label(parent, "BACKLOG", &FONT_LABEL, CYAN);
    lv_obj_set_pos(heading, 18, 15);
    int y = 48;
    int count = 0;
    for (const Mission& mission : state_.missions) {
        if (mission.status != MissionStatus::Backlog) continue;
        lv_obj_t* row = panel(parent, 12, y, width - 36, is_portrait() ? 144 : 116, false);
        render_mission_row(row, mission, false, width - 36);
        y += is_portrait() ? 154 : 126;
        ++count;
    }
    if (count == 0) {
        lv_obj_t* empty = label(parent, "No hay misiones pendientes.", &FONT_BODY, MUTED);
        lv_obj_set_pos(empty, 18, 61);
    }
}

void AppUi::render_mission_row(lv_obj_t* parent, const Mission& mission, bool focus, int width)
{
    lv_obj_t* rank = label(parent, rank_to_string(mission.rank), &FONT_LABEL, rank_color(mission.rank));
    lv_obj_set_pos(rank, 12, 12);
    lv_obj_t* title = label(parent, truncate(mission.title, is_portrait() ? 50 : 32).c_str(), &FONT_LABEL, TEXT);
    lv_obj_set_width(title, width - 80);
    lv_obj_set_pos(title, 42, 11);
    lv_obj_t* elapsed = label(parent, duration_text(service_.current_elapsed(mission)).c_str(), &FONT_LABEL, MUTED);
    lv_obj_set_pos(elapsed, 42, 39);
    if (focus) {
        lv_obj_t* reminder = label(parent, reminder_text(mission).c_str(), &FONT_LABEL, CYAN);
        lv_obj_set_pos(reminder, 140, 39);
        reminder_labels_.push_back({mission.id, reminder});
        const int y = is_portrait() ? 78 : 62;
        button(parent, "INICIAR", 8, y, 82, 48, ActionCode::StartMission, mission.id, 0, nullptr, ACCENT_SOFT, CYAN);
        button(parent, "^", 96, y, 36, 48, ActionCode::MoveUp, mission.id);
        button(parent, "v", 138, y, 36, 48, ActionCode::MoveDown, mission.id);
        button(parent, "EDITAR", 180, y, 58, 48, ActionCode::EditMission, mission.id);
        button(parent, "BORRAR", width - 172, y, 70, 48, ActionCode::AskDelete, mission.id, 0, nullptr,
               DANGER_SOFT, DANGER);
        button(parent, "QUITAR", width - 94, y, 78, 48, ActionCode::RemoveFocus, mission.id);
    } else {
        const int y = is_portrait() ? 78 : 62;
        button(parent, "INICIAR", 12, y, 94, 48, ActionCode::StartMission, mission.id, 0, nullptr, ACCENT_SOFT, CYAN);
        button(parent, "+ ENFOQUE", 114, y, 108, 48, ActionCode::AddFocus, mission.id);
        button(parent, "BORRAR", width - 174, y, 72, 48, ActionCode::AskDelete, mission.id, 0, nullptr,
               DANGER_SOFT, DANGER);
        button(parent, "EDITAR", width - 94, y, 78, 48, ActionCode::EditMission, mission.id);
    }
}

void AppUi::render_history()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    render_header(is_portrait());
    button(screen_, "VOLVER", 18, 102 + kStatusBarHeight, 110, 48, ActionCode::CloseHistory);
    button(screen_, search_query_.empty() ? "BUSCAR" : truncate(search_query_, 18).c_str(), 140, 102 + kStatusBarHeight,
           is_portrait() ? 210 : 300, 48, ActionCode::OpenSearch);
    const char* rank_text = rank_filter_ < 0 ? "RANGO: TODOS" : rank_to_string(rank_from_index(rank_filter_));
    button(screen_, rank_text, is_portrait() ? 18 : 452, is_portrait() ? 160 + kStatusBarHeight : 102 + kStatusBarHeight,
           150, 48, ActionCode::CycleRankFilter);
    const char* date_text = date_filter_days_ == 0 ? "FECHA: TODAS" : date_filter_days_ == 30 ? "ÚLTIMOS 30 DÍAS"
                                                        : date_filter_days_ == 90 ? "ÚLTIMOS 90 DÍAS"
                                                                                  : "ÚLTIMO AÑO";
    button(screen_, date_text, is_portrait() ? 180 : 614,
           is_portrait() ? 160 + kStatusBarHeight : 102 + kStatusBarHeight, 174, 48, ActionCode::CycleDateFilter);
    button(screen_, newest_first_ ? "MÁS RECIENTES" : "MÁS ANTIGUAS", is_portrait() ? 366 : 800,
           is_portrait() ? 160 + kStatusBarHeight : 102 + kStatusBarHeight, 174, 48, ActionCode::ToggleOrder);

    const int list_y = is_portrait() ? 222 + kStatusBarHeight : 164 + kStatusBarHeight;
    lv_obj_t* list = panel(screen_, 18, list_y, width - 36, height - list_y - 18, true);
    std::vector<const Mission*> completed;
    const int64_t cutoff = date_filter_days_ > 0 ? clock_.now_utc() - static_cast<int64_t>(date_filter_days_) * 86400 : 0;
    for (const Mission& mission : state_.missions) {
        if (mission.status != MissionStatus::Completed || !ApplicationService::matches_search(mission, search_query_)) continue;
        if (rank_filter_ >= 0 && mission.rank != rank_from_index(rank_filter_)) continue;
        if (cutoff > 0 && mission.completed_at_utc < cutoff) continue;
        completed.push_back(&mission);
    }
    std::sort(completed.begin(), completed.end(), [this](const Mission* left, const Mission* right) {
        return newest_first_ ? left->completed_at_utc > right->completed_at_utc
                             : left->completed_at_utc < right->completed_at_utc;
    });
    int y = 16;
    const int row_height = is_portrait() ? 132 : 104;
    for (const Mission* mission : completed) {
        lv_obj_t* row = panel(list, 12, y, width - 72, row_height, false);
        lv_obj_t* rank = label(row, rank_to_string(mission->rank), &FONT_LABEL, rank_color(mission->rank));
        lv_obj_set_pos(rank, 14, 15);
        lv_obj_t* title = label(row, mission->title.c_str(), &FONT_LABEL, TEXT);
        lv_obj_set_width(title, width - 370);
        lv_obj_set_pos(title, 48, 13);
        std::string meta = clock_.format_local(mission->completed_at_utc, state_.settings.utc_offset_minutes) +
                           "   " + duration_text(mission->elapsed_seconds) + "   +" +
                           std::to_string(mission->xp_awarded) + " XP";
        lv_obj_t* detail = label(row, meta.c_str(), &FONT_BODY, MUTED);
        lv_obj_set_pos(detail, 48, 43);
        button(row, "DETALLE", width - 276, 24, 92, 48, ActionCode::ShowDetail, mission->id);
        button(row, "ELIMINAR", width - 174, 24, 92, 48, ActionCode::AskDelete, mission->id, 0, nullptr,
               DANGER_SOFT, DANGER);
        y += row_height + 12;
    }
    if (completed.empty()) {
        lv_obj_t* empty = label(list, "No hay misiones completadas para estos filtros.", &FONT_BODY, MUTED);
        lv_obj_set_pos(empty, 24, 32);
    }
}

void AppUi::render_editor(bool search_only)
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    lv_obj_t* title = label(screen_, search_only ? "BUSCAR EN HISTORIAL" : editing_id_ ? "EDITAR MISIÓN" : "NUEVA MISIÓN",
                            &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(title, 20, 18 + kStatusBarHeight);
    button(screen_, "CANCELAR", width - 134, 14 + kStatusBarHeight, 116, 48, ActionCode::CancelEditor);

    const int form_width = width - 40;
    title_textarea_ = lv_textarea_create(screen_);
    place(title_textarea_, 20, 78 + kStatusBarHeight, form_width, search_only ? 76 : 68);
    base_style(title_textarea_, PANEL_ALT, LINE);
    lv_obj_set_style_text_color(title_textarea_, color(TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_textarea_, &FONT_BODY, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(title_textarea_, search_only ? "Título o descripción" : "Título de la misión");
    // Una transcripción larga se conserva para revisión en vez de truncarse sin
    // aviso. SaveMission exige finalmente el límite de dominio de 80 caracteres.
    const size_t title_characters = utf8_codepoint_count(editor_title_);
    lv_textarea_set_max_length(title_textarea_, !search_only && title_characters > 80
                                                       ? static_cast<uint32_t>(std::max<size_t>(240, title_characters))
                                                       : 80);
    lv_textarea_set_one_line(title_textarea_, true);
    lv_textarea_set_text(title_textarea_, search_only ? search_query_.c_str() : editor_title_.c_str());
    lv_obj_add_event_cb(title_textarea_, action_callback, LV_EVENT_FOCUSED, make_action(ActionCode::FocusTitle));

    if (!search_only) {
        if (utf8_codepoint_count(editor_title_) > 80) {
            lv_obj_t* warning = label(screen_, "ACORTA EL TÍTULO A 80 CARACTERES", &FONT_LABEL, DANGER);
            lv_obj_set_pos(warning, 24, 136 + kStatusBarHeight);
        }
        description_textarea_ = lv_textarea_create(screen_);
        place(description_textarea_, 20, 158 + kStatusBarHeight, form_width, 92);
        base_style(description_textarea_, PANEL_ALT, LINE);
        lv_obj_set_style_text_color(description_textarea_, color(TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(description_textarea_, &FONT_BODY, LV_PART_MAIN);
        lv_textarea_set_placeholder_text(description_textarea_, "Descripción opcional");
        lv_textarea_set_max_length(description_textarea_, 240);
        lv_textarea_set_text(description_textarea_, editor_description_.c_str());
        lv_obj_add_event_cb(description_textarea_, action_callback, LV_EVENT_FOCUSED,
                            make_action(ActionCode::FocusDescription));
        const bool portrait = height > width;
        const int rank_y = 258 + kStatusBarHeight;
        lv_obj_t* rank_caption = label(screen_, "RANGO", &FONT_LABEL, CYAN);
        lv_obj_set_pos(rank_caption, 22, rank_y + 8);
        for (int index = 0; index < 6; ++index) {
            const MissionRank rank = rank_from_index(index);
            button(screen_, rank_to_string(rank), 92 + index * 58, rank_y, 48, 48,
                   ActionCode::SelectRank, 0, index, nullptr, editor_rank_ == rank ? ACCENT_SOFT : PANEL_ALT,
                   editor_rank_ == rank ? rank_color(rank) : LINE);
        }
        lv_obj_t* reminder_caption = label(screen_, "RECORDATORIO", &FONT_LABEL, CYAN);
        const int reminder_y = portrait ? rank_y + 58 : rank_y;
        const int reminder_x = portrait ? 22 : 470;
        const int reminder_control_x = portrait ? 190 : 610;
        lv_obj_set_pos(reminder_caption, reminder_x, reminder_y + 8);
        button(screen_, editor_reminder_enabled_ ? "ACTIVO" : "APAGADO", reminder_control_x, reminder_y, 100, 48,
               ActionCode::ReminderToggle, 0, 0, nullptr, editor_reminder_enabled_ ? ACCENT_SOFT : PANEL_ALT,
               editor_reminder_enabled_ ? CYAN : LINE);
        const int reminder_minus_x = portrait ? 304 : 722;
        const int reminder_value_x = portrait ? 356 : 780;
        const int reminder_plus_x = portrait ? 438 : 860;
        button(screen_, "-", reminder_minus_x, reminder_y, 46, 48, ActionCode::ReminderMinus);
        char minutes[32];
        std::snprintf(minutes, sizeof(minutes), "%u MIN", editor_reminder_minutes_);
        reminder_value_label_ = label(screen_, minutes, &FONT_LABEL, TEXT);
        lv_obj_set_pos(reminder_value_label_, reminder_value_x, reminder_y + 14);
        button(screen_, "+", reminder_plus_x, reminder_y, 46, 48, ActionCode::ReminderPlus);
    }

    // El teclado queda acoplado al borde inferior. Las acciones del formulario
    // ocupan una franja inmediatamente encima y el contenido nunca se dibuja por
    // debajo del teclado, incluso en la orientación horizontal.
    const int keyboard_height = height >= 1000 ? 404 : 292;
    const int keyboard_y = height - keyboard_height - 20;
    const int action_y = keyboard_y - 56;
    keyboard_ = spanish_keyboard(24, keyboard_y, width - 48, keyboard_height);

    button(screen_, "MICRÓFONO", 20, action_y, 150, 48, ActionCode::OpenDictation);
    button(screen_, search_only ? "APLICAR BÚSQUEDA" : "GUARDAR MISIÓN", width - 216, action_y, 198, 48,
           search_only ? ActionCode::ApplySearch : ActionCode::SaveMission, 0, 0, nullptr, ACCENT_SOFT, CYAN);

    set_active_textarea(title_textarea_);
}

void AppUi::render_dictation_setup()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    const ConnectivitySnapshot snapshot = dictation_.connectivity_snapshot();
    const BleSnapshot ble = dictation_.ble_snapshot();
    last_connectivity_generation_ = snapshot.generation;
    last_ble_generation_ = ble.generation;
    lv_obj_t* title = label(screen_, network_editor_open_ ? "AGREGAR RED" : "CONECTIVIDAD", &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(title, 20, 16 + kStatusBarHeight);
    button(screen_, network_editor_open_ ? "CANCELAR" : "VOLVER", width - 140, 12 + kStatusBarHeight, 120, 48,
           network_editor_open_ ? ActionCode::CancelNetworkEditor : ActionCode::CloseConnectivity);

    if (network_editor_open_) {
        const int keyboard_height = height >= 1000 ? 404 : 292;
        const int keyboard_y = height - keyboard_height - 20;
        const int action_y = keyboard_y - 56;
        lv_obj_t* form = panel(screen_, 18, 76 + kStatusBarHeight, width - 36,
                               action_y - 86 - kStatusBarHeight, false);
        const int field_width = width - 84;
        lv_obj_t* ssid_caption = label(form, "NOMBRE DE LA RED", &FONT_LABEL, CYAN);
        lv_obj_set_pos(ssid_caption, 24, 18);
        ssid_textarea_ = lv_textarea_create(form);
        place(ssid_textarea_, 24, 46, field_width, 56);
        base_style(ssid_textarea_, PANEL_ALT, LINE);
        lv_obj_set_style_text_font(ssid_textarea_, &FONT_BODY, LV_PART_MAIN);
        lv_obj_set_style_text_color(ssid_textarea_, color(TEXT), LV_PART_MAIN);
        lv_textarea_set_one_line(ssid_textarea_, true);
        lv_textarea_set_max_length(ssid_textarea_, 32);
        lv_textarea_set_text(ssid_textarea_, network_editor_.ssid.c_str());
        lv_obj_add_event_cb(ssid_textarea_, action_callback, LV_EVENT_FOCUSED,
                            make_action(ActionCode::FocusNetworkField, 0, 0));
        if (network_editor_secured_) {
            lv_obj_t* password_caption = label(form, "CONTRASEÑA", &FONT_LABEL, CYAN);
            lv_obj_set_pos(password_caption, 24, 116);
            password_textarea_ = lv_textarea_create(form);
            place(password_textarea_, 24, 144, field_width, 56);
            base_style(password_textarea_, PANEL_ALT, LINE);
            lv_obj_set_style_text_font(password_textarea_, &FONT_BODY, LV_PART_MAIN);
            lv_obj_set_style_text_color(password_textarea_, color(TEXT), LV_PART_MAIN);
            lv_textarea_set_one_line(password_textarea_, true);
            lv_textarea_set_max_length(password_textarea_, 63);
            lv_textarea_set_password_mode(password_textarea_, true);
            lv_textarea_set_text(password_textarea_, network_editor_.password.c_str());
            lv_obj_add_event_cb(password_textarea_, action_callback, LV_EVENT_FOCUSED,
                                make_action(ActionCode::FocusNetworkField, 0, 1));
        }
        const std::string form_note = network_connect_pending_
            ? "Comprobando la red; todavía no se guardó la contraseña."
            : snapshot.error.empty() ? "La red solo se guardará después de obtener conexión." : snapshot.error;
        lv_obj_t* note = label(form, form_note.c_str(), &FONT_BODY, snapshot.error.empty() ? MUTED : DANGER);
        lv_obj_set_pos(note, 24, network_editor_secured_ ? 214 : 116);
        lv_obj_set_width(note, field_width);

        button(screen_, network_connect_pending_ ? "CONECTANDO…" : "CONECTAR Y GUARDAR", 24, action_y,
               std::min(270, width / 2), 48, ActionCode::ConnectAndSaveNetwork, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
        keyboard_ = spanish_keyboard(24, keyboard_y, width - 48, keyboard_height);
        set_active_textarea(network_editor_.ssid.empty() ? ssid_textarea_ :
                            (network_editor_secured_ ? password_textarea_ : ssid_textarea_));
        return;
    }

    const int tabs_y = 72 + kStatusBarHeight;
    const int tab_width = std::min(210, (width - 52) / 3);
    const char* tab_names[] = {"REDES", "MACBOOK", "ESTADO"};
    for (int index = 0; index < 3; ++index) {
        button(screen_, tab_names[index], 18 + index * (tab_width + 8), tabs_y, tab_width, 48,
               ActionCode::ConnectivityTab, 0, index, nullptr,
               connectivity_tab_ == index ? ACCENT_SOFT : PANEL_ALT,
               connectivity_tab_ == index ? CYAN : LINE);
    }
    const int content_y = tabs_y + 58;
    lv_obj_t* content = panel(screen_, 18, content_y, width - 36, height - content_y - 18, true);
    const int inner_width = width - 72;
    const std::string status_text = snapshot.error.empty() ? snapshot.message : snapshot.error;
    lv_obj_t* state_label = label(content, connectivity_phase_name(snapshot.phase), &FONT_LABEL,
                                  snapshot.error.empty() ? CYAN : DANGER);
    lv_obj_set_pos(state_label, 20, 16);
    lv_obj_t* message = label(content, status_text.c_str(), &FONT_BODY,
                              snapshot.error.empty() ? MUTED : DANGER);
    lv_obj_set_pos(message, 190, 14);
    lv_obj_set_width(message, inner_width - 210);
    int y = 62;

    if (connectivity_tab_ == 0) {
        label(content, "REDES CONOCIDAS", &FONT_LABEL, TEXT);
        lv_obj_t* heading = lv_obj_get_child(content, lv_obj_get_child_count(content) - 1);
        lv_obj_set_pos(heading, 20, y);
        button(content, "BUSCAR", inner_width - 130, y - 10, 112, 44, ActionCode::ScanNetworks);
        y += 40;
        if (snapshot.profiles.empty()) {
            lv_obj_t* empty = label(content, "Todavía no hay redes guardadas.", &FONT_BODY, MUTED);
            lv_obj_set_pos(empty, 20, y + 12);
            y += 58;
        }
        for (size_t index = 0; index < snapshot.profiles.size(); ++index) {
            const KnownNetwork& profile = snapshot.profiles[index];
            lv_obj_t* row = panel(content, 20, y, inner_width - 40, 66, false);
            const std::string caption = std::to_string(index + 1) + ". " + profile.ssid +
                                        (profile.current ? "  · CONECTADA" : profile.hidden ? "  · OCULTA" : "");
            lv_obj_t* name = label(row, caption.c_str(), &FONT_BODY, profile.current ? CYAN : TEXT);
            lv_obj_set_pos(name, 14, 20);
            const int controls_x = inner_width - 390;
            button(row, "USAR", controls_x, 9, 76, 48, ActionCode::ConnectKnownNetwork, 0,
                   static_cast<int>(index), nullptr, profile.current ? ACCENT_SOFT : PANEL_ALT,
                   profile.current ? CYAN : LINE);
            button(row, "^", controls_x + 84, 9, 44, 48, ActionCode::MoveNetworkPriority, 0,
                   static_cast<int>(index * 2));
            button(row, "v", controls_x + 136, 9, 44, 48, ActionCode::MoveNetworkPriority, 0,
                   static_cast<int>(index * 2 + 1));
            button(row, "OLVIDAR", controls_x + 188, 9, 96, 48, ActionCode::ForgetNetwork, 0,
                   static_cast<int>(index), nullptr, DANGER_SOFT, DANGER);
            y += 76;
        }
        button(content, "+ RED OCULTA", 20, y, 150, 48, ActionCode::OpenHiddenNetwork);
        y += 70;
        lv_obj_t* available = label(content, "DISPONIBLES", &FONT_LABEL, TEXT);
        lv_obj_set_pos(available, 20, y);
        y += 34;
        for (size_t index = 0; index < snapshot.networks.size(); ++index) {
            const WifiNetwork& network = snapshot.networks[index];
            std::string caption = network.ssid + "  · " + std::to_string(network.rssi) + " dBm";
            if (network.known) caption += "  · GUARDADA";
            button(content, caption.c_str(), 20, y, inner_width - 40, 48, ActionCode::SelectNetwork, 0,
                   static_cast<int>(index), nullptr, network.known ? ACCENT_SOFT : PANEL_ALT,
                   network.known ? CYAN : LINE);
            y += 56;
        }
    } else if (connectivity_tab_ == 1) {
        lv_obj_t* help = label(content,
                               "El MacBook se detecta automáticamente mediante Bonjour. Inicia servidor.sh y mantén ambos equipos en la misma red.",
                               &FONT_BODY, MUTED);
        lv_obj_set_pos(help, 20, y);
        lv_obj_set_width(help, inner_width - 180);
        button(content, "ACTUALIZAR", inner_width - 150, y, 132, 48, ActionCode::RefreshServers);
        y += 78;
        if (snapshot.servers.empty()) {
            lv_obj_t* empty = label(content, "No se encontró ningún servidor compatible.", &FONT_BODY, MUTED);
            lv_obj_set_pos(empty, 20, y);
        }
        for (size_t index = 0; index < snapshot.servers.size(); ++index) {
            const DiscoveredServer& server = snapshot.servers[index];
            lv_obj_t* row = panel(content, 20, y, inner_width - 40, 82, false);
            lv_obj_t* name = label(row, server.name.c_str(), &FONT_DISPLAY, server.selected ? CYAN : TEXT);
            lv_obj_set_pos(name, 16, 10);
            const std::string detail = server.ipv4 + ":" + std::to_string(server.port) +
                                       (server.selected ? "  · PREFERIDO" : "");
            lv_obj_t* address = label(row, detail.c_str(), &FONT_BODY, MUTED);
            lv_obj_set_pos(address, 18, 46);
            button(row, server.selected ? "SELECCIONADO" : "ELEGIR", inner_width - 210, 17, 150, 48,
                   ActionCode::SelectServer, 0, static_cast<int>(index), nullptr,
                   server.selected ? ACCENT_SOFT : PANEL_ALT, server.selected ? CYAN : LINE);
            y += 92;
        }
    } else {
        std::string wifi = snapshot.connected() ? snapshot.current_ssid + "  · " +
                          std::to_string(snapshot.current_rssi) + " dBm" : "Sin conexión Wi‑Fi";
        lv_obj_t* wifi_title = label(content, "RED ACTUAL", &FONT_LABEL, CYAN);
        lv_obj_set_pos(wifi_title, 20, y);
        lv_obj_t* wifi_value = label(content, wifi.c_str(), &FONT_DISPLAY, TEXT);
        lv_obj_set_pos(wifi_value, 20, y + 30);
        y += 92;
        lv_obj_t* mac_title = label(content, "SERVIDOR DE DICTADO", &FONT_LABEL, CYAN);
        lv_obj_set_pos(mac_title, 20, y);
        const std::string mac = snapshot.selected_server_name.empty() ? "Sin MacBook seleccionado"
                                                                      : snapshot.selected_server_name;
        lv_obj_t* mac_value = label(content, mac.c_str(), &FONT_DISPLAY, TEXT);
        lv_obj_set_pos(mac_value, 20, y + 30);
        y += 98;
        lv_obj_t* ble_title = label(content, "BLUETOOTH · PRUEBA A0", &FONT_LABEL, CYAN);
        lv_obj_set_pos(ble_title, 20, y);
        const std::string ble_text = std::string(ble_phase_name(ble.phase)) + "  · " +
                                     (ble.error.empty() ? ble.message : ble.error) +
                                     "  · PINGS " + std::to_string(ble.probe_count) +
                                     "  · RAM " + std::to_string(ble.free_internal_bytes / 1024) + " KB";
        lv_obj_t* ble_value = label(content, ble_text.c_str(), &FONT_DISPLAY,
                                    ble.phase == BlePhase::Error ? DANGER : TEXT);
        lv_obj_set_pos(ble_value, 20, y + 30);
        lv_obj_set_width(ble_value, inner_width - 40);
        y += 98;
        button(content, "RECONECTAR", 20, y, 148, 48, ActionCode::ReconnectNetworks, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
        button(content, "BUSCAR MAC", 180, y, 148, 48, ActionCode::RefreshServers);
        button(content, "PROBAR WHISPER", 340, y, 174, 48, ActionCode::TestWhisperServer);
    }
}

void AppUi::capture_network_credentials()
{
    if (ssid_textarea_ != nullptr) network_editor_.ssid = lv_textarea_get_text(ssid_textarea_);
    if (password_textarea_ != nullptr) network_editor_.password = lv_textarea_get_text(password_textarea_);
}

void AppUi::apply_dictation_candidate()
{
    if (view_ != View::Editor) open_editor(0);
    editor_title_ = dictation_candidate_;
    dictation_candidate_.clear();
    overlay_ = Overlay::None;
}

lv_obj_t* AppUi::spanish_keyboard(int x, int y, int width, int height)
{
    lv_obj_t* dock = lv_obj_create(screen_);
    place(dock, x, y, width, height);
    lv_obj_set_style_bg_color(dock, color(PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dock, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dock, theme::RadiusPanel, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dock, 0, LV_PART_MAIN);
    lv_obj_remove_flag(dock, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* matrix = lv_buttonmatrix_create(dock);
    place(matrix, 0, 0, width - 16, height - 16);
    lv_obj_center(matrix);
    lv_obj_add_event_cb(matrix, action_callback, LV_EVENT_VALUE_CHANGED, make_action(ActionCode::KeyboardKey));

    lv_obj_set_style_bg_color(matrix, color(PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(matrix, theme::RadiusPanel, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_row(matrix, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_column(matrix, 5, LV_PART_MAIN);

    lv_obj_set_style_bg_color(matrix, color(PANEL_ALT), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, color(LINE), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, theme::BorderWidth, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, theme::RadiusControl, LV_PART_ITEMS);
    lv_obj_set_style_text_font(matrix, &keyboard_font(), LV_PART_ITEMS);
    lv_obj_set_style_text_color(matrix, color(TEXT), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(matrix, color(VIOLET), static_cast<lv_style_selector_t>(LV_PART_ITEMS) |
                                                       static_cast<lv_style_selector_t>(LV_STATE_PRESSED));
    // La tecla de mayusculas se resalta mientras esta activa.
    lv_obj_set_style_bg_color(matrix, color(ACCENT_SOFT), static_cast<lv_style_selector_t>(LV_PART_ITEMS) |
                                                       static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_set_style_border_color(matrix, color(CYAN), static_cast<lv_style_selector_t>(LV_PART_ITEMS) |
                                                       static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    lv_obj_set_style_text_color(matrix, color(CYAN), static_cast<lv_style_selector_t>(LV_PART_ITEMS) |
                                                     static_cast<lv_style_selector_t>(LV_STATE_CHECKED));
    keyboard_ = matrix;
    update_keyboard_map();
    return matrix;
}

void AppUi::update_keyboard_map()
{
    if (keyboard_ == nullptr) return;

    const char* const* map = nullptr;
    if (keyboard_page_ == KeyboardPage::Letters) {
        map = keyboard_upper_ ? kKeyboardUpper : kKeyboardLower;
    } else if (keyboard_page_ == KeyboardPage::Numbers) {
        map = keyboard_upper_ ? kKeyboardNumbersUpper : kKeyboardNumbersLower;
    } else {
        map = kKeyboardSymbols;
    }
    lv_buttonmatrix_set_map(keyboard_, map);

    const uint32_t shift_id = keyboard_page_ == KeyboardPage::Letters ? 20
                              : keyboard_page_ == KeyboardPage::Numbers ? 32 : UINT32_MAX;
    const uint32_t count = keyboard_page_ == KeyboardPage::Letters ? 34
                           : keyboard_page_ == KeyboardPage::Numbers ? 36 : 37;
    for (uint32_t index = 0; index < count; ++index) {
        const char* key = lv_buttonmatrix_get_button_text(keyboard_, index);
        if (key == nullptr) continue;
        uint8_t width = 1;
        if (keyboard_page_ == KeyboardPage::Letters) {
            if (std::strcmp(key, LV_SYMBOL_UP) == 0) width = 2;
            else if (std::strcmp(key, LV_SYMBOL_BACKSPACE) == 0) width = 3;
            else if (std::strcmp(key, "ESPACIO") == 0) width = 6;
            else if (std::strcmp(key, LV_SYMBOL_NEW_LINE) == 0 || std::strcmp(key, "123") == 0) width = 3;
        } else {
            if (std::strcmp(key, LV_SYMBOL_UP) == 0) width = 3;
            else if (std::strcmp(key, LV_SYMBOL_BACKSPACE) == 0) width = 4;
            else if (std::strcmp(key, "ESPACIO") == 0) width = 7;
            else if (std::strcmp(key, LV_SYMBOL_NEW_LINE) == 0 || std::strcmp(key, "123") == 0 ||
                     std::strcmp(key, "ABC") == 0 || std::strcmp(key, "#+=") == 0) width = 3;
        }
        lv_buttonmatrix_set_button_width(keyboard_, index, width);
    }
    if (keyboard_upper_ && shift_id != UINT32_MAX) {
        lv_buttonmatrix_set_button_ctrl(keyboard_, shift_id, LV_BUTTONMATRIX_CTRL_CHECKED);
    } else if (shift_id != UINT32_MAX) {
        lv_buttonmatrix_clear_button_ctrl(keyboard_, shift_id, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
}

void AppUi::handle_key(const char* text)
{
    if (text == nullptr || active_textarea_ == nullptr) return;
    if (std::strcmp(text, "123") == 0) {
        keyboard_page_ = KeyboardPage::Numbers;
        update_keyboard_map();
        return;
    }
    if (std::strcmp(text, "#+=") == 0) {
        keyboard_page_ = KeyboardPage::Symbols;
        update_keyboard_map();
        return;
    }
    if (std::strcmp(text, "ABC") == 0) {
        keyboard_page_ = KeyboardPage::Letters;
        update_keyboard_map();
        return;
    }
    if (std::strcmp(text, LV_SYMBOL_BACKSPACE) == 0) {
        lv_textarea_delete_char(active_textarea_);
        return;
    }
    if (std::strcmp(text, "ESPACIO") == 0) {
        lv_textarea_add_text(active_textarea_, " ");
        return;
    }
    if (std::strcmp(text, LV_SYMBOL_NEW_LINE) == 0) {
        // Las areas de una linea ignoran el salto (el titulo); la descripcion lo usa.
        lv_textarea_add_text(active_textarea_, "\n");
        return;
    }
    if (std::strcmp(text, LV_SYMBOL_UP) == 0) {
        keyboard_upper_ = !keyboard_upper_;
        update_keyboard_map();
        return;
    }
    lv_textarea_add_text(active_textarea_, text);
}

void AppUi::set_active_textarea(lv_obj_t* target)
{
    active_textarea_ = target;
    // Marca en cual de los dos campos va a escribir el teclado.
    if (title_textarea_ != nullptr) {
        const bool on = target == title_textarea_;
        lv_obj_set_style_border_color(title_textarea_, color(on ? CYAN : LINE), LV_PART_MAIN);
        lv_obj_set_style_border_width(title_textarea_, on ? 2 : 1, LV_PART_MAIN);
    }
    if (description_textarea_ != nullptr) {
        const bool on = target == description_textarea_;
        lv_obj_set_style_border_color(description_textarea_, color(on ? CYAN : LINE), LV_PART_MAIN);
        lv_obj_set_style_border_width(description_textarea_, on ? 2 : 1, LV_PART_MAIN);
    }
}

void AppUi::render_clock_setup()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);

    lv_obj_t* title = label(screen_, "CONFIGURAR FECHA Y HORA", &FONT_DISPLAY, TEXT);
    lv_obj_set_pos(title, 30, 24 + kStatusBarHeight);
    const std::string help_text = "Desliza cada columna para ajustar. Zona: " + state_.settings.timezone + ".";
    lv_obj_t* help = label(screen_, help_text.c_str(), &FONT_BODY, MUTED);
    lv_obj_set_pos(help, 32, 68 + kStatusBarHeight);
    lv_obj_set_width(help, width - 64);

    // Cinco ruedas deslizables en lugar de un teclado: no hay digitos que
    // memorizar ni teclas pequenas que acertar.
    static const char* const kCaptions[5] = {"DÍA", "MES", "AÑO", "HORA", "MINUTO"};
    static const int kWeights[5] = {17, 25, 19, 15, 15};
    constexpr int kWeightTotal = 91;
    constexpr int kGap = 22;
    const int available = std::max(5 * 80, width - 60 - kGap * 4);
    int positions[5];
    int widths[5];
    int cursor = std::max(30, (width - (available + kGap * 4)) / 2);
    for (int index = 0; index < 5; ++index) {
        widths[index] = std::max(80, available * kWeights[index] / kWeightTotal);
        positions[index] = cursor;
        cursor += widths[index] + kGap;
    }

    constexpr int kCaptionY = 120 + kStatusBarHeight;
    constexpr int kRollerY = 152 + kStatusBarHeight;
    for (int index = 0; index < 5; ++index) {
        lv_obj_t* caption = label(screen_, kCaptions[index], &FONT_LABEL, CYAN);
        lv_obj_set_pos(caption, positions[index] + 4, kCaptionY);
    }

    clock_day_count_ = days_in_month(clock_year_, clock_month_);
    clock_day_roller_ =
        clock_roller(positions[0], kRollerY, widths[0], day_options(clock_day_count_), clock_day_ - 1);
    clock_month_roller_ = clock_roller(positions[1], kRollerY, widths[1], month_options(), clock_month_ - 1);
    clock_year_roller_ =
        clock_roller(positions[2], kRollerY, widths[2], year_options(), clock_year_ - kClockYearBase);
    clock_hour_roller_ = clock_roller(positions[3], kRollerY, widths[3], numeric_options(24), clock_hour_);
    clock_minute_roller_ = clock_roller(positions[4], kRollerY, widths[4], numeric_options(60), clock_minute_);

    // El mes y el año determinan cuantos dias ofrece la rueda de la izquierda.
    lv_obj_add_event_cb(clock_month_roller_, action_callback, LV_EVENT_VALUE_CHANGED,
                        make_action(ActionCode::RefreshClockDays));
    lv_obj_add_event_cb(clock_year_roller_, action_callback, LV_EVENT_VALUE_CHANGED,
                        make_action(ActionCode::RefreshClockDays));

    const int rollers_bottom = kRollerY + clock_roller_height();
    const int action_y = rollers_bottom + 72;

    lv_obj_t* zone = label(screen_, "ZONA HORARIA", &FONT_LABEL, CYAN);
    lv_obj_set_pos(zone, 32, rollers_bottom + 38);

    char offset[32];
    const int hours = state_.settings.utc_offset_minutes / 60;
    const int minutes = std::abs(state_.settings.utc_offset_minutes % 60);
    std::snprintf(offset, sizeof(offset), "UTC%+d:%02d", hours, minutes);
    button(screen_, "-", 30, action_y, 64, 64, ActionCode::ClockOffsetMinus);
    lv_obj_t* offset_label = label(screen_, offset, &FONT_LABEL, CYAN);
    lv_obj_set_pos(offset_label, 114, action_y + 21);
    button(screen_, "+", 246, action_y, 64, 64, ActionCode::ClockOffsetPlus);
    lv_obj_t* step_hint = label(screen_, "Pasos de 30 min", &FONT_BODY, MUTED);
    lv_obj_set_pos(step_hint, 330, action_y + 25);
    button(screen_, "GUARDAR Y CONTINUAR", width - 372, action_y, 340, 64, ActionCode::SaveClock, 0, 0, nullptr,
           ACCENT_SOFT, CYAN);

    lv_obj_t* note = label(screen_,
                           "Valor inicial: fecha de compilación del firmware. La hora solo fecha los intervalos; el "
                           "cronómetro mide con el reloj monotónico.",
                           &FONT_BODY, MUTED);
    lv_obj_set_pos(note, 32, height - 74);
    lv_obj_set_width(note, width - 380);
}

lv_obj_t* AppUi::clock_roller(int x, int y, int width, const std::string& options, int selected)
{
    lv_obj_t* roller = lv_roller_create(screen_);
    lv_obj_set_pos(roller, x, y);
    lv_obj_set_width(roller, width);
    lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller, kClockVisibleRows);
    lv_obj_set_style_text_line_space(roller, kClockRollerLineSpace, LV_PART_MAIN);
    lv_obj_set_height(roller, clock_roller_height());
    lv_obj_set_style_bg_color(roller, color(PANEL_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, color(LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, theme::BorderWidth, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, theme::RadiusControl, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &FONT_DISPLAY, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, color(MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, color(ACCENT_SOFT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &FONT_DISPLAY, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, color(TEXT), LV_PART_SELECTED);
    lv_roller_set_selected(roller, static_cast<uint32_t>(std::max(0, selected)), LV_ANIM_OFF);
    return roller;
}

void AppUi::seed_clock_defaults()
{
    if (clock_defaults_ready_) return;
    std::tm value {};
    if (clock_.is_valid()) {
        const int64_t local_utc = clock_.now_utc() + state_.settings.utc_offset_minutes * 60;
        const time_t seconds = static_cast<time_t>(local_utc);
        gmtime_r(&seconds, &value);
    } else {
        build_time(value);
    }
    clock_year_ = std::clamp(value.tm_year + 1900, kClockYearBase, kClockYearBase + kClockYearCount - 1);
    clock_month_ = std::clamp(value.tm_mon + 1, 1, 12);
    clock_day_ = std::clamp(value.tm_mday, 1, days_in_month(clock_year_, clock_month_));
    clock_hour_ = std::clamp(value.tm_hour, 0, 23);
    clock_minute_ = std::clamp(value.tm_min, 0, 59);
    clock_defaults_ready_ = true;
}

void AppUi::capture_clock()
{
    if (clock_day_roller_ != nullptr) clock_day_ = static_cast<int>(lv_roller_get_selected(clock_day_roller_)) + 1;
    if (clock_month_roller_ != nullptr) {
        clock_month_ = static_cast<int>(lv_roller_get_selected(clock_month_roller_)) + 1;
    }
    if (clock_year_roller_ != nullptr) {
        clock_year_ = kClockYearBase + static_cast<int>(lv_roller_get_selected(clock_year_roller_));
    }
    if (clock_hour_roller_ != nullptr) clock_hour_ = static_cast<int>(lv_roller_get_selected(clock_hour_roller_));
    if (clock_minute_roller_ != nullptr) clock_minute_ = static_cast<int>(lv_roller_get_selected(clock_minute_roller_));
    clock_day_ = std::clamp(clock_day_, 1, days_in_month(clock_year_, clock_month_));
}

void AppUi::update_clock_days()
{
    if (clock_day_roller_ == nullptr || clock_month_roller_ == nullptr || clock_year_roller_ == nullptr) return;
    clock_month_ = static_cast<int>(lv_roller_get_selected(clock_month_roller_)) + 1;
    clock_year_ = kClockYearBase + static_cast<int>(lv_roller_get_selected(clock_year_roller_));
    const int days = days_in_month(clock_year_, clock_month_);
    const int previous = static_cast<int>(lv_roller_get_selected(clock_day_roller_)) + 1;
    clock_day_ = std::clamp(previous, 1, days);
    // El evento llega en cada fin de desplazamiento; si el mes no cambia de
    // longitud no hay nada que reconstruir y la rueda no parpadea.
    if (clock_day_count_ == days) return;
    clock_day_count_ = days;
    lv_roller_set_options(clock_day_roller_, day_options(days).c_str(), LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(clock_day_roller_, static_cast<uint32_t>(clock_day_ - 1), LV_ANIM_OFF);
}

void AppUi::render_screen_off()
{
    lv_obj_t* shade = lv_obj_create(screen_);
    place(shade, 0, 0, lv_display_get_horizontal_resolution(display_), lv_display_get_vertical_resolution(display_));
    lv_obj_set_style_bg_color(shade, color(SCRIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shade, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(shade, 0, LV_PART_MAIN);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shade, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(shade, action_callback, LV_EVENT_CLICKED, make_action(ActionCode::WakeScreen));
}

void AppUi::render_overlay()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    lv_obj_t* shade = lv_obj_create(screen_);
    place(shade, 0, 0, width, height);
    lv_obj_set_style_bg_color(shade, color(SCRIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shade, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(shade, 0, LV_PART_MAIN);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    const int box_width = std::min(width - 56, 680);
    const int box_height = overlay_ == Overlay::MissionDetail ? std::min(height - 80, 510)
                           : overlay_ == Overlay::Dictation   ? std::min(height - 56, 500)
                                                              : 360;
    lv_obj_t* box = panel(shade, (width - box_width) / 2, (height - box_height) / 2, box_width, box_height, true);

    if (overlay_ == Overlay::ConfirmTetrisNew) {
        auto* title = label(box, "¿NUEVA PARTIDA?", &FONT_DISPLAY, CYAN);
        lv_obj_set_pos(title, 24, 24);
        auto* text = label(box, "Se descartará la partida actual. Tu récord se conserva.", &FONT_DISPLAY, TEXT);
        lv_obj_set_pos(text, 24, 82); lv_obj_set_width(text, box_width - 48);
        button(box, "NUEVA PARTIDA", 24, 202, box_width - 48, 58, ActionCode::TetrisConfirmNew);
        button(box, "CANCELAR", 24, 274, box_width - 48, 58, ActionCode::CloseOverlay);
        return;
    }

    if (overlay_ == Overlay::RadarPick) {
        lv_obj_t* title = label(box, "ELIGE EL EMISOR", &FONT_LABEL, CYAN);
        lv_obj_set_pos(title, 18, 12);
        int y = 58;
        for (uint32_t id : radar_hits_) {
            const auto found = std::find_if(radar_snapshot_.emitters.begin(), radar_snapshot_.emitters.end(),
                                           [id](const RadarEmitter& e) { return e.id == id; });
            if (found == radar_snapshot_.emitters.end()) continue;
            const std::string text = truncate(found->observation.name, 28) + " · " + radar_address(found->observation);
            button(box, text.c_str(), 18, y, box_width-36, 52, ActionCode::RadarSelect, id);
            y += 60;
        }
        button(box, "CANCELAR", 18, y, box_width-36, 52, ActionCode::CloseOverlay);
        return;
    }

    if (overlay_ == Overlay::Dictation) {
        const DictationSnapshot snapshot = dictation_.snapshot();
        lv_obj_t* heading = label(box, "DICTADO DE MISIÓN", &FONT_DISPLAY, CYAN);
        lv_obj_set_pos(heading, 24, 22);
        dictation_status_label_ = label(box, dictation_phase_name(snapshot.phase), &FONT_LABEL,
                                        snapshot.phase == DictationPhase::Error ? DANGER : TEXT);
        lv_obj_set_pos(dictation_status_label_, 24, 70);
        char elapsed[24];
        std::snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu.%01lu",
                      static_cast<unsigned long>(snapshot.recorded_ms / 60000),
                      static_cast<unsigned long>((snapshot.recorded_ms / 1000) % 60),
                      static_cast<unsigned long>((snapshot.recorded_ms / 100) % 10));
        dictation_time_label_ = label(box, elapsed, &FONT_NUMBER, CYAN);
        lv_obj_set_pos(dictation_time_label_, box_width - 180, 54);
        dictation_level_bar_ = lv_bar_create(box);
        place(dictation_level_bar_, 24, 112, box_width - 48, 22);
        lv_bar_set_range(dictation_level_bar_, 0, 100);
        lv_bar_set_value(dictation_level_bar_, static_cast<int>(std::clamp(snapshot.level * 450.0f, 0.0f, 100.0f)),
                         LV_ANIM_OFF);
        lv_obj_set_style_bg_color(dictation_level_bar_, color(PANEL_ALT), LV_PART_MAIN);
        lv_obj_set_style_bg_color(dictation_level_bar_, color(CYAN), LV_PART_INDICATOR);

        std::string content = snapshot.error.empty() ? snapshot.message : snapshot.error;
        if (snapshot.phase == DictationPhase::Recording) {
            char diagnostics[720];
            const float strongest_peak = *std::max_element(snapshot.slot_peaks.begin(), snapshot.slot_peaks.end());
            std::snprintf(diagnostics, sizeof(diagnostics),
                          "\n\nTDM 48 kHz · bloque %lu bytes · %s\n"
                          "S0 RMS %.6f  PK %.6f  [%d, %d]  NZ %u/960\n"
                          "S1 RMS %.6f  PK %.6f  [%d, %d]  NZ %u/960\n"
                          "S2 RMS %.6f  PK %.6f  [%d, %d]  NZ %u/960\n"
                          "S3 RMS %.6f  PK %.6f  [%d, %d]  NZ %u/960\n"
                          "Mapa confirmado: MIC S%u/S%u · REF S%u",
                          static_cast<unsigned long>(snapshot.bytes_read),
                          strongest_peak < 0.000001f ? "SIN DATOS DE AUDIO" : "SEÑAL RECIBIDA",
                          snapshot.slot_levels[0], snapshot.slot_peaks[0], snapshot.slot_min[0], snapshot.slot_max[0],
                          static_cast<unsigned>(snapshot.slot_nonzero[0]), snapshot.slot_levels[1],
                          snapshot.slot_peaks[1], snapshot.slot_min[1], snapshot.slot_max[1],
                          static_cast<unsigned>(snapshot.slot_nonzero[1]),
                          snapshot.slot_levels[2], snapshot.slot_peaks[2], snapshot.slot_min[2], snapshot.slot_max[2],
                          static_cast<unsigned>(snapshot.slot_nonzero[2]), snapshot.slot_levels[3],
                          snapshot.slot_peaks[3], snapshot.slot_min[3], snapshot.slot_max[3],
                          static_cast<unsigned>(snapshot.slot_nonzero[3]),
                          snapshot.microphone_slot_a, snapshot.microphone_slot_b, snapshot.reference_slot);
            content += diagnostics;
        } else if (snapshot.phase == DictationPhase::LocalResult) {
            char slots[120];
            std::snprintf(slots, sizeof(slots), "Mapa confirmado: MIC S%u/S%u · REF S%u",
                          snapshot.microphone_slot_a, snapshot.microphone_slot_b, snapshot.reference_slot);
            content += "\n\n" + std::string(slots);
        } else if (snapshot.phase == DictationPhase::Result) {
            content += "\n\n“" + snapshot.candidate + "”";
        }
        lv_obj_t* body = label(box, content.c_str(), &FONT_BODY,
                               snapshot.phase == DictationPhase::Error ? DANGER : TEXT);
        lv_obj_set_pos(body, 24, 156);
        lv_obj_set_width(body, box_width - 48);

        const int y = box_height - 70;
        if (snapshot.phase == DictationPhase::Unconfigured) {
            button(box, "CONFIGURAR", 24, y, 150, 48, ActionCode::OpenConnectivity, 0, 0, nullptr, ACCENT_SOFT, CYAN);
            button(box, "CANCELAR", box_width - 142, y, 116, 48, ActionCode::DictationCancel);
        } else if (snapshot.phase == DictationPhase::Recording) {
            button(box, "DETENER", 24, y, 130, 48, ActionCode::DictationStop, 0, 0, nullptr, ACCENT_SOFT, CYAN);
            button(box, "CANCELAR", box_width - 142, y, 116, 48, ActionCode::DictationCancel);
        } else if (snapshot.phase == DictationPhase::Result) {
            button(box, "USAR COMO TÍTULO", 24, y, 190, 48, ActionCode::DictationAccept, 0, 0, nullptr,
                   ACCENT_SOFT, CYAN);
            button(box, "DESCARTAR", box_width - 142, y, 116, 48, ActionCode::DictationCancel);
        } else if (snapshot.phase == DictationPhase::Error || snapshot.phase == DictationPhase::LocalResult) {
            if (dictation_.has_audio()) {
                button(box, "REPRODUCIR", 24, y, 132, 48, ActionCode::PlayLastCapture);
                if (dictation_.wifi_configured()) {
                    button(box, "REINTENTAR", 166, y, 132, 48, ActionCode::DictationRetry, 0, 0, nullptr,
                           ACCENT_SOFT, CYAN);
                }
            }
            button(box, "CONFIGURAR", box_width - 274, y, 126, 48, ActionCode::OpenConnectivity);
            button(box, "DESCARTAR", box_width - 138, y, 112, 48, ActionCode::DictationCancel);
        } else {
            button(box, "CANCELAR", box_width - 142, y, 116, 48, ActionCode::DictationCancel);
        }
        return;
    }

    std::string title_text = overlay_title_;
    std::string body_text = overlay_body_;
    if (overlay_ == Overlay::Tutorial) {
        title_text = "BIENVENIDO A EL SISTEMA";
        body_text = "1. Crea o elige una misión.\n\n2. Añádela a la Zona de Enfoque.\n\n3. Cambia de misión cuando una tarea quede esperando. El Sistema pausará la anterior y conservará cada intervalo.\n\nCompletar misiones concede XP.";
    } else if (overlay_ == Overlay::Reminder) {
        const Mission* mission = find_mission(state_, overlay_mission_id_);
        title_text = "RECORDATORIO DE MISIÓN";
        body_text = mission == nullptr ? "Una misión pausada requiere atención." : mission->title;
    } else if (overlay_ == Overlay::ConfirmDelete) {
        title_text = "ELIMINAR MISIÓN";
        const Mission* mission = find_mission(state_, overlay_mission_id_);
        body_text = mission != nullptr && mission->status == MissionStatus::Completed
                        ? "La misión desaparecerá del historial. La XP ya obtenida no se restará."
                        : "La misión y sus intervalos desaparecerán de forma permanente.";
    } else if (overlay_ == Overlay::ConfirmForgetNetwork) {
        title_text = "OLVIDAR RED";
        const ConnectivitySnapshot network = dictation_.connectivity_snapshot();
        if (pending_forget_network_ >= 0 &&
            static_cast<size_t>(pending_forget_network_) < network.profiles.size()) {
            const KnownNetwork& profile = network.profiles[static_cast<size_t>(pending_forget_network_)];
            body_text = profile.current
                ? "Se desconectará de “" + profile.ssid + "” y buscará otra red conocida. ¿Quieres continuar?"
                : "Se eliminarán las credenciales guardadas de “" + profile.ssid + "”. ¿Quieres continuar?";
        } else {
            body_text = "La lista de redes cambió. Cierra este aviso y vuelve a intentarlo.";
        }
    } else if (overlay_ == Overlay::ConfirmDictationReplace) {
        title_text = "REEMPLAZAR TÍTULO";
        body_text = "El editor ya contiene un título. ¿Quieres reemplazarlo por la transcripción?";
    } else if (overlay_ == Overlay::ConfirmHourglassReset) {
        title_text = "REINICIAR RELOJ";
        body_text = "El tiempo volverá a cero y se preparará una nueva sesión. Tendrás que pulsar Iniciar.";
    } else if (overlay_ == Overlay::MissionDetail) {
        const Mission* mission = find_mission(state_, overlay_mission_id_);
        if (mission != nullptr) {
            title_text = mission->title;
            body_text = mission->description + "\n\nRango " + rank_to_string(mission->rank) + " · " +
                        duration_text(mission->elapsed_seconds) + " · +" + std::to_string(mission->xp_awarded) +
                        " XP\nCompletada: " +
                        clock_.format_local(mission->completed_at_utc, state_.settings.utc_offset_minutes) +
                        "\n\nINTERVALOS";
            for (const ActiveInterval& interval : mission->intervals) {
                body_text += "\n- " + clock_.format_local(interval.started_at_utc, state_.settings.utc_offset_minutes) +
                             " · " + duration_text(interval.duration_seconds);
            }
        }
    }

    lv_obj_t* heading = label(box, title_text.c_str(), &FONT_DISPLAY,
                              overlay_ == Overlay::Reward ? GOLD : CYAN);
    lv_obj_set_width(heading, box_width - 48);
    lv_obj_set_pos(heading, 24, 24);
    if (overlay_ == Overlay::Reward) {
        // No se anima transform_scale: en el ESP32-P4 esa transformacion obliga
        // al renderer a crear una capa intermedia y puede dejar bloqueado el
        // pipeline PPA al reconstruir la pantalla. La opacidad conserva el
        // efecto de entrada sin usar capas transformadas.
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, heading);
        lv_anim_set_values(&animation, LV_OPA_20, LV_OPA_COVER);
        lv_anim_set_duration(&animation, 360);
        lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
            lv_obj_set_style_text_opa(static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(value), LV_PART_MAIN);
        });
        lv_anim_start(&animation);
    }
    lv_obj_t* body = label(box, body_text.c_str(), &FONT_BODY, TEXT);
    lv_obj_set_width(body, box_width - 48);
    lv_obj_set_pos(body, 24, 82);

    const int y = box_height - 70;
    if (overlay_ == Overlay::Tutorial) {
        button(box, "COMENZAR", box_width - 178, y, 150, 48, ActionCode::FinishTutorial, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
    } else if (overlay_ == Overlay::Reward) {
        if (suggested_next_id_ != 0)
            button(box, "INICIAR SIGUIENTE", 24, y, 184, 48, ActionCode::StartSuggested, suggested_next_id_, 0,
                   nullptr, ACCENT_SOFT, CYAN);
        button(box, "VOLVER", box_width - 144, y, 116, 48, ActionCode::CloseOverlay);
    } else if (overlay_ == Overlay::Reminder) {
        button(box, "RETOMAR", 24, y, 126, 48, ActionCode::ReminderResume, overlay_mission_id_, 0, nullptr,
               ACCENT_SOFT, CYAN);
        button(box, "+5 MIN", 160, y, 112, 48, ActionCode::ReminderSnooze, overlay_mission_id_);
        button(box, "CANCELAR AVISO", box_width - 180, y, 152, 48, ActionCode::ReminderCancel,
               overlay_mission_id_);
    } else if (overlay_ == Overlay::ConfirmDelete) {
        button(box, "CONSERVAR", 24, y, 130, 48, ActionCode::CloseOverlay);
        button(box, "ELIMINAR", box_width - 144, y, 116, 48, ActionCode::ConfirmDelete, overlay_mission_id_, 0,
               nullptr, DANGER_SOFT, DANGER);
    } else if (overlay_ == Overlay::ConfirmForgetNetwork) {
        button(box, "CONSERVAR", 24, y, 130, 48, ActionCode::CloseOverlay);
        button(box, "OLVIDAR", box_width - 144, y, 116, 48, ActionCode::ConfirmForgetNetwork, 0, 0,
               nullptr, DANGER_SOFT, DANGER);
    } else if (overlay_ == Overlay::ConfirmDictationReplace) {
        button(box, "CONSERVAR", 24, y, 130, 48, ActionCode::CloseOverlay);
        button(box, "REEMPLAZAR", box_width - 158, y, 130, 48, ActionCode::ConfirmDictationReplace, 0, 0,
               nullptr, ACCENT_SOFT, CYAN);
    } else if (overlay_ == Overlay::ConfirmHourglassReset) {
        button(box, "CONSERVAR", 24, y, 130, 48, ActionCode::CloseOverlay);
        button(box, "REINICIAR", box_width - 158, y, 130, 48, ActionCode::HourglassReset,
               0, 0, nullptr, PANEL_ALT, theme::HourglassGold);
    } else {
        button(box, "CERRAR", box_width - 144, y, 116, 48, ActionCode::CloseOverlay);
    }
}

void AppUi::dispatch(const Action& action)
{
    bool rerender = true;
    switch (action.code) {
        case ActionCode::NewMission:
            open_editor(0);
            break;
        case ActionCode::EditMission:
            open_editor(action.id);
            break;
        case ActionCode::SaveMission: {
            capture_editor();
            if (editor_title_.empty()) {
                show_info("Título requerido", "Escribe un título antes de guardar la misión.");
                break;
            }
            if (utf8_codepoint_count(editor_title_) > 80) {
                show_info("Título demasiado largo",
                          "La transcripción se conservó completa. Acórtala a 80 caracteres antes de guardar.");
                break;
            }
            if (utf8_codepoint_count(editor_description_) > 240) {
                show_info("Descripción demasiado larga", "Acórtala a 240 caracteres antes de guardar.");
                break;
            }
            bool ok = false;
            if (editing_id_ == 0) {
                ok = service_.create_mission(editor_title_, editor_description_, editor_rank_, editor_reminder_enabled_,
                                             editor_reminder_minutes_) != 0;
            } else {
                ok = service_.update_mission(editing_id_, editor_title_, editor_description_, editor_rank_,
                                             editor_reminder_enabled_, editor_reminder_minutes_);
            }
            if (ok) view_ = View::Dashboard;
            else show_info("No se pudo guardar", repository_.last_error());
            break;
        }
        case ActionCode::CancelEditor:
            view_ = view_ == View::SearchEditor ? View::History : View::Dashboard;
            break;
        case ActionCode::AddFocus: service_.add_to_focus(action.id); break;
        case ActionCode::StartMission: service_.start(action.id); break;
        case ActionCode::PauseMission: service_.pause_active(); break;
        case ActionCode::CompleteMission: {
            uint32_t xp = 0;
            MissionId next = 0;
            ESP_LOGI(TAG_UI, "Completando mision activa %lu", static_cast<unsigned long>(state_.focus.active_id));
            if (service_.complete_active(xp, next)) {
                ESP_LOGI(TAG_UI, "Mision completada: +%lu XP; siguiente %lu", static_cast<unsigned long>(xp),
                         static_cast<unsigned long>(next));
                overlay_title_ = "MISIÓN COMPLETADA";
                overlay_body_ = "+" + std::to_string(xp) + " XP obtenida\nNivel " +
                                std::to_string(state_.profile.level);
                suggested_next_id_ = next;
                overlay_ = Overlay::Reward;
            } else {
                ESP_LOGE(TAG_UI, "No se pudo completar la mision: %s", repository_.last_error());
                show_info("No se pudo completar", repository_.last_error());
            }
            break;
        }
        case ActionCode::RemoveFocus: service_.remove_from_focus(action.id); break;
        case ActionCode::MoveUp: service_.move_in_focus(action.id, -1); break;
        case ActionCode::MoveDown: service_.move_in_focus(action.id, 1); break;
        case ActionCode::OpenAssistant:
            open_module(ModuleId::Assistant);
            break;
        case ActionCode::CloseAssistant:
            assistant_voice_.cancel();
            assistant_.close(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            view_ = View::Home;
            break;
        case ActionCode::OpenModule:
            open_module(static_cast<ModuleId>(action.value));
            break;
        case ActionCode::GoHome:
            if (view_ == View::Maze) leave_maze();
            if (view_ == View::Tetris) leave_tetris();
            if (view_ == View::Radar) { radar_.visible(false); radar_visible_ = false; }
            if (view_ == View::Hourglass || view_ == View::HourglassSetup) leave_hourglass();
            if (view_ == View::Assistant) {
                assistant_voice_.cancel();
                assistant_.close(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            }
            overlay_ = Overlay::None;
            view_ = View::Home;
            break;
        case ActionCode::MazeStart:
            if (!maze_.start()) show_info("Laberinto ocupado", "Vuelve a intentarlo en unos segundos.");
            rerender=false;break;
        case ActionCode::MazeResume:
            maze_.resume();rerender=false;break;
        case ActionCode::MazePause:
            maze_.pause();rerender=false;break;
        case ActionCode::MazeCalibrate:
            maze_.calibrate();rerender=false;break;
        case ActionCode::MazeSelect:
            maze_.select(action.value);rerender=false;break;
        case ActionCode::TetrisNew:
            if (tetris_snapshot_.state == TetrisState::Paused || tetris_snapshot_.state == TetrisState::Running) {
                tetris_.pause(); overlay_ = Overlay::ConfirmTetrisNew;
            } else if (tetris_.start()) { tetris_playing_ = true; tetris_sound_menu_ = false; }
            else show_info("Tetris ocupado", "Vuelve a intentarlo en unos segundos.");
            break;
        case ActionCode::TetrisConfirmNew:
            if (tetris_.start()) { overlay_ = Overlay::None; tetris_playing_ = true; tetris_sound_menu_ = false; }
            else show_info("Tetris ocupado", "Vuelve a intentarlo en unos segundos.");
            break;
        case ActionCode::TetrisContinue:
            if (tetris_.resume()) { tetris_playing_ = true; tetris_sound_menu_ = false; }
            break;
        case ActionCode::TetrisPause:
            tetris_.pause(); tetris_gesture_.cancel(); rerender = false;
            break;
        case ActionCode::TetrisSound:
            tetris_sound_menu_ = !tetris_sound_menu_;
            break;
        case ActionCode::TetrisMusic:
            tetris_.settings(!tetris_snapshot_.music, tetris_snapshot_.effects, tetris_snapshot_.volume);
            rerender = false;
            break;
        case ActionCode::TetrisEffects:
            tetris_.settings(tetris_snapshot_.music, !tetris_snapshot_.effects, tetris_snapshot_.volume);
            rerender = false;
            break;
        case ActionCode::TetrisVolume:
            tetris_.settings(tetris_snapshot_.music, tetris_snapshot_.effects,
                             std::clamp<int>(tetris_snapshot_.volume + action.value, 0, 100));
            rerender = false;
            break;
        case ActionCode::RadarFilter:
            radar_.filter(static_cast<el_sistema::RadarFilter>(action.value));
            radar_page_ = 0; radar_next_update_ms_ = 0; rerender = false;
            break;
        case ActionCode::RadarPause:
            radar_.pause(!radar_snapshot_.paused); radar_next_update_ms_ = 0; rerender = false;
            break;
        case ActionCode::RadarSound:
            radar_.sound(!radar_snapshot_.sound); radar_next_update_ms_ = 0; rerender = false;
            break;
        case ActionCode::RadarSelect:
            radar_.select(action.id); radar_next_update_ms_ = 0;
            if (overlay_ == Overlay::RadarPick) overlay_ = Overlay::None;
            else rerender = false;
            break;
        case ActionCode::RadarPage:
            radar_page_ = std::max(0, radar_page_ + action.value); radar_next_update_ms_ = 0; rerender = false;
            break;
        case ActionCode::HourglassPreset:
            hourglass_selected_minutes_ = static_cast<uint16_t>(action.value);
            if (hourglass_duration_roller_) lv_roller_set_selected(hourglass_duration_roller_, action.value - 1,
                                                                  LV_ANIM_ON);
            rerender = false;
            break;
        case ActionCode::HourglassStart:
            if (hourglass_duration_roller_) {
                hourglass_selected_minutes_ = lv_roller_get_selected(hourglass_duration_roller_) + 1;
            }
            hourglass_pending_command_ = hourglass_.start(hourglass_selected_minutes_);
            if (hourglass_pending_command_) hourglass_start_pending_ = true;
            else show_info("El reloj está ocupado", "Vuelve a intentarlo en unos segundos.");
            rerender = overlay_ != Overlay::None;
            break;
        case ActionCode::HourglassToggleControls:
            hourglass_controls_open_ = !hourglass_controls_open_;
            hourglass_controls_since_us_ = esp_timer_get_time();
            if (hourglass_controls_) {
                if (hourglass_controls_open_) lv_obj_remove_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
                else lv_obj_add_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
            }
            rerender = false;
            break;
        case ActionCode::HourglassPauseResume: {
            const HourglassSnapshot state = hourglass_.snapshot();
            bool accepted = true;
            if (state.state == HourglassState::Running) accepted = hourglass_.pause();
            else if (state.state == HourglassState::Paused) accepted = hourglass_.resume();
            else if (state.state == HourglassState::Ready) accepted = hourglass_.start(state.duration_minutes);
            else {
                // A completed clock is armed already. Hide controls and wait for
                // the physical inversion; do not refill or reset it here.
                hourglass_controls_open_ = false;
                if (hourglass_controls_) lv_obj_add_flag(hourglass_controls_, LV_OBJ_FLAG_HIDDEN);
            }
            if (!accepted) show_info("El reloj está ocupado", "Vuelve a intentarlo en unos segundos.");
            rerender = !accepted;
            break;
        }
        case ActionCode::HourglassAskReset:
            overlay_ = Overlay::ConfirmHourglassReset;
            break;
        case ActionCode::HourglassReset:
            if (!hourglass_.reset()) show_info("El reloj está ocupado", "Vuelve a intentarlo en unos segundos.");
            else overlay_ = Overlay::None;
            hourglass_controls_open_ = true;
            break;
        case ActionCode::HourglassDuration:
            hourglass_.pause();
            hourglass_selected_minutes_ = hourglass_.snapshot().duration_minutes;
            hourglass_config_from_session_ = true;
            view_ = View::HourglassSetup;
            break;
        case ActionCode::HourglassCancelSetup:
            hourglass_start_pending_ = false;
            if (hourglass_config_from_session_) {
                hourglass_controls_open_ = true;
                view_ = View::Hourglass;
            } else leave_hourglass();
            break;
        case ActionCode::AssistantTap:
            assistant_.tap(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            refresh_assistant_visual();
            rerender = false;
            break;
        case ActionCode::AssistantDemo:
            assistant_voice_.cancel();
            assistant_.start_visual_demo(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            refresh_assistant_visual();
            rerender = false;
            break;
        case ActionCode::AssistantCelebrate:
            assistant_voice_.cancel();
            assistant_.celebrate(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            refresh_assistant_visual();
            rerender = false;
            break;
        case ActionCode::AssistantVoiceStart:
            if (!dictation_.wifi_configured() || dictation_.connectivity_snapshot().server_selection_required()) {
                connectivity_return_view_ = View::Assistant;
                connectivity_tab_ = dictation_.wifi_configured() ? 1 : 0;
                view_ = View::DictationSetup;
                break;
            }
            if (!assistant_voice_.start()) {
                show_info("Audio ocupado", "Espera a que termine la operación de audio actual e inténtalo de nuevo.");
            }
            break;
        case ActionCode::AssistantVoiceStop:
            assistant_voice_.stop();
            rerender = false;
            break;
        case ActionCode::AssistantVoiceCancel:
            assistant_voice_.cancel();
            assistant_.finish_turn(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            break;
        case ActionCode::AssistantVoiceRetry:
            if (!assistant_voice_.retry()) {
                show_info("No se puede reintentar", "La muestra de voz ya no está disponible.");
            }
            break;
        case ActionCode::OpenHistory:
            view_ = View::History;
            break;
        case ActionCode::CloseHistory:
            view_ = View::Dashboard;
            break;
        case ActionCode::ToggleSidePanel:
            // Tocar la pestana activa colapsa la lista y devuelve el ancho completo a
            // la mision activa.
            side_panel_ = side_panel_ == action.value ? -1 : action.value;
            break;
        case ActionCode::OpenSearch:
            view_ = View::SearchEditor;
            break;
        case ActionCode::ApplySearch:
            search_query_ = title_textarea_ == nullptr ? "" : lv_textarea_get_text(title_textarea_);
            view_ = View::History;
            break;
        case ActionCode::CycleRankFilter:
            rank_filter_ = rank_filter_ >= 5 ? -1 : rank_filter_ + 1;
            break;
        case ActionCode::CycleDateFilter:
            if (action.value == -10 || action.value == -11) {
                portrait_tab_ = action.value == -10 ? 0 : 1;
            } else {
                date_filter_days_ = date_filter_days_ == 0 ? 30 : date_filter_days_ == 30 ? 90
                                                          : date_filter_days_ == 90   ? 365
                                                                                      : 0;
            }
            break;
        case ActionCode::ToggleOrder: newest_first_ = !newest_first_; break;
        case ActionCode::ShowDetail:
            overlay_mission_id_ = action.id;
            overlay_ = Overlay::MissionDetail;
            break;
        case ActionCode::AskDelete:
            overlay_mission_id_ = action.id;
            overlay_ = Overlay::ConfirmDelete;
            break;
        case ActionCode::ConfirmDelete:
            service_.delete_mission(action.id);
            overlay_ = Overlay::None;
            if (view_ == View::Editor) view_ = View::Dashboard;
            break;
        case ActionCode::CloseOverlay:
            overlay_ = Overlay::None;
            overlay_mission_id_ = 0;
            pending_forget_network_ = -1;
            break;
        case ActionCode::StartSuggested:
            service_.start(action.id);
            overlay_ = Overlay::None;
            break;
        case ActionCode::ReminderResume:
            if (view_ == View::Hourglass || view_ == View::HourglassSetup) leave_hourglass(View::Dashboard);
            service_.resume_from_reminder(action.id);
            overlay_ = Overlay::None;
            break;
        case ActionCode::ReminderSnooze:
            service_.snooze_reminder(action.id, 5);
            overlay_ = Overlay::None;
            break;
        case ActionCode::ReminderCancel:
            service_.cancel_reminder(action.id);
            overlay_ = Overlay::None;
            break;
        case ActionCode::SaveClock:
            save_clock();
            break;
        case ActionCode::ClockOffsetMinus:
            capture_clock();
            state_.settings.utc_offset_minutes = std::max<int>(-720, state_.settings.utc_offset_minutes - 30);
            break;
        case ActionCode::ClockOffsetPlus:
            capture_clock();
            state_.settings.utc_offset_minutes = std::min<int>(840, state_.settings.utc_offset_minutes + 30);
            break;
        case ActionCode::RefreshClockDays:
            update_clock_days();
            rerender = false;
            break;
        case ActionCode::WakeScreen:
            set_screen_off(false);
            rerender = false;
            break;
        case ActionCode::PowerOffDevice:
            power_off_device();
            break;
        case ActionCode::FinishTutorial:
            state_.settings.tutorial_completed = true;
            service_.save();
            overlay_ = Overlay::None;
            break;
        case ActionCode::OpenDictation:
            dictation_from_editor_ = view_ == View::Editor;
            connectivity_return_view_ = dictation_from_editor_ ? View::Editor : View::Dashboard;
            if (dictation_from_editor_) capture_editor();
            if (!dictation_.wifi_configured() || dictation_.connectivity_snapshot().server_selection_required()) {
                overlay_ = Overlay::None;
                connectivity_tab_ = dictation_.wifi_configured() ? 1 : 0;
                view_ = View::DictationSetup;
                break;
            }
            overlay_ = Overlay::Dictation;
            dictation_.start();
            last_dictation_phase_ = dictation_.snapshot().phase;
            break;
        case ActionCode::DictationStop:
            dictation_.stop();
            rerender = false;
            break;
        case ActionCode::DictationCancel:
            dictation_.cancel();
            dictation_.discard();
            overlay_ = Overlay::None;
            break;
        case ActionCode::DictationRetry:
            dictation_.retry();
            break;
        case ActionCode::DictationAccept:
            dictation_candidate_ = dictation_.accept_result();
            if (dictation_from_editor_ && !editor_title_.empty() && editor_title_ != dictation_candidate_) {
                overlay_ = Overlay::ConfirmDictationReplace;
            } else {
                apply_dictation_candidate();
            }
            break;
        case ActionCode::ConfirmDictationReplace:
            apply_dictation_candidate();
            break;
        case ActionCode::OpenConnectivity:
            dictation_from_editor_ = view_ == View::Editor;
            connectivity_return_view_ = view_;
            if (view_ == View::Assistant && assistant_voice_.busy()) assistant_voice_.cancel();
            if (dictation_from_editor_) capture_editor();
            dictation_.cancel();
            overlay_ = Overlay::None;
            network_editor_open_ = false;
            network_connect_pending_ = false;
            view_ = View::DictationSetup;
            break;
        case ActionCode::CloseConnectivity:
            view_ = connectivity_return_view_;
            break;
        case ActionCode::ConnectivityTab:
            connectivity_tab_ = std::clamp(action.value, 0, 2);
            break;
        case ActionCode::ScanNetworks:
            dictation_.scan_networks();
            break;
        case ActionCode::OpenHiddenNetwork:
            network_editor_ = {};
            network_editor_.hidden = true;
            network_editor_secured_ = true;
            network_editor_open_ = true;
            network_connect_pending_ = false;
            break;
        case ActionCode::CancelNetworkEditor:
            network_editor_open_ = false;
            network_connect_pending_ = false;
            network_editor_ = {};
            break;
        case ActionCode::SelectNetwork:
            if (action.value >= 0) {
                const ConnectivitySnapshot network = dictation_.connectivity_snapshot();
                if (static_cast<size_t>(action.value) < network.networks.size()) {
                    const WifiNetwork& selected = network.networks[action.value];
                    auto known = std::find_if(network.profiles.begin(), network.profiles.end(),
                                              [&](const KnownNetwork& profile) { return profile.ssid == selected.ssid; });
                    if (known != network.profiles.end()) {
                        dictation_.connect_known(static_cast<size_t>(known - network.profiles.begin()));
                    } else {
                        network_editor_ = {selected.ssid, "", false};
                        network_editor_secured_ = selected.secured;
                        network_editor_open_ = true;
                        network_connect_pending_ = false;
                    }
                }
            }
            break;
        case ActionCode::ConnectAndSaveNetwork:
            if (network_connect_pending_) {
                rerender = false;
                break;
            }
            capture_network_credentials();
            network_connect_pending_ = dictation_.connect_and_save(network_editor_);
            break;
        case ActionCode::ConnectKnownNetwork:
            if (action.value >= 0) dictation_.connect_known(static_cast<size_t>(action.value));
            break;
        case ActionCode::ForgetNetwork:
            if (action.value >= 0) {
                pending_forget_network_ = action.value;
                overlay_ = Overlay::ConfirmForgetNetwork;
            }
            break;
        case ActionCode::ConfirmForgetNetwork:
            if (pending_forget_network_ >= 0)
                dictation_.forget_network(static_cast<size_t>(pending_forget_network_));
            pending_forget_network_ = -1;
            overlay_ = Overlay::None;
            break;
        case ActionCode::MoveNetworkPriority:
            if (action.value >= 0)
                dictation_.move_network(static_cast<size_t>(action.value / 2), action.value % 2 == 0 ? -1 : 1);
            break;
        case ActionCode::ReconnectNetworks:
            dictation_.reconnect_networks();
            break;
        case ActionCode::RefreshServers:
            dictation_.discover_servers();
            break;
        case ActionCode::SelectServer: {
            const ConnectivitySnapshot network = dictation_.connectivity_snapshot();
            if (action.value >= 0 && static_cast<size_t>(action.value) < network.servers.size())
                dictation_.select_server(network.servers[action.value].id);
            break;
        }
        case ActionCode::TestWhisperServer:
            dictation_.test_server();
            break;
        case ActionCode::FocusNetworkField:
            if (action.value == 0) set_active_textarea(ssid_textarea_);
            else set_active_textarea(password_textarea_);
            rerender = false;
            break;
        case ActionCode::PlayLastCapture:
            if (!dictation_.play_last_capture()) show_info("No se pudo reproducir", "Revisa el codec y vuelve a intentar.");
            break;
        case ActionCode::KeyboardKey: {
            if (keyboard_ != nullptr) {
                const uint32_t button_id = lv_buttonmatrix_get_selected_button(keyboard_);
                if (button_id != LV_BUTTONMATRIX_BUTTON_NONE) {
                    handle_key(lv_buttonmatrix_get_button_text(keyboard_, button_id));
                }
            }
            rerender = false;
            break;
        }
        case ActionCode::SelectRank:
            capture_editor();
            editor_rank_ = rank_from_index(action.value);
            break;
        case ActionCode::ReminderMinus:
            capture_editor();
            editor_reminder_minutes_ = std::max<int>(1, editor_reminder_minutes_ - 1);
            break;
        case ActionCode::ReminderPlus:
            capture_editor();
            editor_reminder_minutes_ = std::min<int>(120, editor_reminder_minutes_ + 1);
            break;
        case ActionCode::ReminderToggle:
            capture_editor();
            editor_reminder_enabled_ = !editor_reminder_enabled_;
            break;
        case ActionCode::FocusTitle:
            set_active_textarea(title_textarea_);
            rerender = false;
            break;
        case ActionCode::FocusDescription:
            set_active_textarea(description_textarea_);
            rerender = false;
            break;
    }
    if (rerender) schedule_render();
}

void AppUi::tick()
{
    // Boton fisico (GPIO35): pulsacion corta enciende o apaga la pantalla,
    // mantenida apaga el equipo.
    for (ButtonEvent event = button_.next_event(); event != ButtonEvent::None; event = button_.next_event()) {
        if (event == ButtonEvent::ShortPress) {
            set_screen_off(!screen_off_);
        } else if (event == ButtonEvent::LongPress) {
            power_off_device();
            return;
        }
    }

    const ConnectivitySnapshot connectivity_snapshot = dictation_.connectivity_snapshot();
    if (connectivity_snapshot.generation != last_connectivity_generation_) {
        last_connectivity_generation_ = connectivity_snapshot.generation;
        if (view_ == View::DictationSetup && network_editor_open_ && network_connect_pending_) {
            if (connectivity_snapshot.phase == ConnectivityPhase::ConnectionFailed ||
                connectivity_snapshot.phase == ConnectivityPhase::Error) {
                network_connect_pending_ = false;
                schedule_render();
                return;
            }
            const bool saved = std::any_of(connectivity_snapshot.profiles.begin(),
                                           connectivity_snapshot.profiles.end(), [&](const KnownNetwork& profile) {
                return profile.ssid == network_editor_.ssid;
            });
            const bool connection_finished = connectivity_snapshot.phase == ConnectivityPhase::Connected ||
                                             connectivity_snapshot.phase == ConnectivityPhase::DiscoveringServer ||
                                             connectivity_snapshot.phase == ConnectivityPhase::Ready ||
                                             connectivity_snapshot.phase == ConnectivityPhase::ServerMissing;
            if (connection_finished && saved && connectivity_snapshot.current_ssid == network_editor_.ssid) {
                network_connect_pending_ = false;
                network_editor_open_ = false;
                network_editor_ = {};
                schedule_render();
                return;
            }
            schedule_render();
            return;
        }
        if (view_ == View::DictationSetup) {
            schedule_render();
            return;
        }
    }

    const BleSnapshot ble_snapshot = dictation_.ble_snapshot();
    if (ble_snapshot.generation != last_ble_generation_) {
        last_ble_generation_ = ble_snapshot.generation;
        if (view_ == View::DictationSetup && connectivity_tab_ == 2) {
            schedule_render();
            return;
        }
    }

    assistant_voice_.poll();
    const DictationSnapshot dictation_snapshot = dictation_.snapshot();
    const std::string dictation_message = dictation_snapshot.error.empty() ? dictation_snapshot.message
                                                                            : dictation_snapshot.error;
    if (dictation_snapshot.phase != last_dictation_phase_ || dictation_message != last_dictation_message_) {
        last_dictation_phase_ = dictation_snapshot.phase;
        last_dictation_message_ = dictation_message;
        if (overlay_ == Overlay::Dictation || view_ == View::DictationSetup) {
            schedule_render();
            return;
        }
    } else if (overlay_ == Overlay::Dictation) {
        if (dictation_time_label_ != nullptr) {
            char elapsed[24];
            std::snprintf(elapsed, sizeof(elapsed), "%02lu:%02lu.%01lu",
                          static_cast<unsigned long>(dictation_snapshot.recorded_ms / 60000),
                          static_cast<unsigned long>((dictation_snapshot.recorded_ms / 1000) % 60),
                          static_cast<unsigned long>((dictation_snapshot.recorded_ms / 100) % 10));
            lv_label_set_text(dictation_time_label_, elapsed);
        }
        if (dictation_level_bar_ != nullptr) {
            lv_bar_set_value(dictation_level_bar_,
                             static_cast<int>(std::clamp(dictation_snapshot.level * 450.0f, 0.0f, 100.0f)),
                             LV_ANIM_OFF);
        }
    }

    const uint64_t assistant_now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    const AssistantVoiceSnapshot voice_snapshot = assistant_voice_.snapshot();
    if (voice_snapshot.generation != last_assistant_voice_generation_) {
        last_assistant_voice_generation_ = voice_snapshot.generation;
        switch (voice_snapshot.phase) {
            case AssistantVoicePhase::Listening: assistant_.begin_listening(assistant_now_ms); break;
            case AssistantVoicePhase::Thinking: assistant_.begin_thinking(assistant_now_ms); break;
            case AssistantVoicePhase::Speaking: assistant_.begin_speaking(assistant_now_ms); break;
            case AssistantVoicePhase::Complete: assistant_.finish_turn(assistant_now_ms); break;
            case AssistantVoicePhase::Error: assistant_.fail_turn(assistant_now_ms); break;
            case AssistantVoicePhase::Idle: break;
        }
        if (view_ == View::Assistant) {
            schedule_render();
            return;
        }
    }
    assistant_.update(assistant_now_ms);
    if (view_ == View::Assistant) {
        last_assistant_generation_ = assistant_.snapshot().generation;
        refresh_assistant_visual();
    }

    static int64_t last_second = -1;
    const int64_t now_second = clock_.now_utc();
    if (now_second != last_second) {
        last_second = now_second;
        refresh_statusbar();
        const Mission* active = find_mission(state_, state_.focus.active_id);
        if (active != nullptr) {
            const uint32_t elapsed = service_.current_elapsed(*active);
            if (timer_label_ != nullptr) lv_label_set_text(timer_label_, duration_text(elapsed).c_str());
            const int checkpoint_slot = static_cast<int>(elapsed / 30);
            if (checkpoint_slot != last_checkpoint_second_) {
                last_checkpoint_second_ = checkpoint_slot;
                service_.checkpoint();
            }
        }
        for (const auto& [id, reminder_label] : reminder_labels_) {
            const Mission* mission = find_mission(state_, id);
            if (mission != nullptr && reminder_label != nullptr) {
                lv_label_set_text(reminder_label, reminder_text(*mission).c_str());
            }
        }
        if (overlay_ == Overlay::None && !dictation_.busy() && !assistant_voice_.busy() &&
            (view_ != View::Hourglass || hourglass_controls_open_)) {
            const std::vector<MissionId> due = service_.due_reminders();
            if (!due.empty()) {
                if (view_ == View::Maze) {
                    const auto game=maze_.snapshot();
                    if (!game.available) return;
                    if (game.state==MazeState::Running || game.state==MazeState::Calibrating) {
                        maze_.pause();return; // Never show reminders over a moving ball.
                    }
                }
                if (view_ == View::Tetris) {
                    const auto game = tetris_.snapshot();
                    if (!game.available) return;
                    if (game.state == TetrisState::Running) {
                        tetris_.pause(); tetris_gesture_.cancel();
                        return; // Show only after the worker confirms the pause.
                    }
                }
                overlay_mission_id_ = due.front();
                overlay_ = Overlay::Reminder;
                schedule_render();
                return;
            }
        }
    }

    const bool rotation_locked = view_ == View::Editor || view_ == View::SearchEditor || view_ == View::ClockSetup ||
                                 view_ == View::DictationSetup || view_ == View::Hourglass ||
                                 view_ == View::HourglassSetup || view_ == View::Tetris || view_ == View::Maze || overlay_ != Overlay::None || screen_off_ ||
                                 dictation_.busy() || assistant_voice_.busy() || home_rotation_active_ || render_pending_;
    const int next = orientation_.poll();
    if (!rotation_locked
#ifdef EL_SISTEMA_RADAR_SMOKE
        && !radar_validation_active_
#endif
    ) {
        last_rotation_lock_ = -1;
        if (next >= 0 && next != current_rotation_) set_rotation(next);
    } else if (next >= 0 && next != current_rotation_ && next != last_rotation_lock_) {
        last_rotation_lock_ = next;
        ESP_LOGI(TAG_UI, "Rotación %d pendiente hasta cerrar formulario, diálogo o actividad", next);
    }
}

void AppUi::open_editor(MissionId id)
{
    editing_id_ = id;
    editor_title_.clear();
    editor_description_.clear();
    editor_rank_ = MissionRank::E;
    editor_reminder_enabled_ = true;
    editor_reminder_minutes_ = state_.settings.default_reminder_minutes;
    if (const Mission* mission = find_mission(state_, id)) {
        editor_title_ = mission->title;
        editor_description_ = mission->description;
        editor_rank_ = mission->rank;
        editor_reminder_enabled_ = mission->reminder.enabled;
        editor_reminder_minutes_ = mission->reminder.delay_minutes;
    }
    view_ = View::Editor;
}

void AppUi::capture_editor()
{
    if (title_textarea_ != nullptr) editor_title_ = lv_textarea_get_text(title_textarea_);
    if (description_textarea_ != nullptr) editor_description_ = lv_textarea_get_text(description_textarea_);
}

void AppUi::show_info(const std::string& title_text, const std::string& body_text)
{
    overlay_title_ = title_text;
    overlay_body_ = body_text;
    overlay_ = Overlay::Info;
}

void AppUi::save_clock()
{
    // Las ruedas ya limitan cada campo a un valor legal (incluidos los dias de
    // cada mes y los años bisiestos), asi que aqui no hay digitos que validar.
    capture_clock();
    if (!clock_.set_local(clock_year_, clock_month_, clock_day_, clock_hour_, clock_minute_,
                          state_.settings.utc_offset_minutes)) {
        show_info("Fecha no válida", "El RTC rechazó la fecha. Revisa los valores y vuelve a intentarlo.");
        return;
    }
    service_.save();
    view_ = View::Home;
}

void AppUi::open_module(ModuleId module)
{
    const ModuleDescriptor* descriptor = find_module(module);
    if (descriptor == nullptr) return;
    overlay_ = Overlay::None;
    if (module == ModuleId::Maze) {
        maze_return_rotation_=current_rotation_;current_rotation_=0;orientation_.set_current(0);
        lv_display_set_rotation(display_,LV_DISPLAY_ROTATION_0);home_rotation_pending_=false;
        view_=View::Maze;return;
    }
    if (module == ModuleId::Tetris) {
        tetris_return_rotation_ = current_rotation_;
        current_rotation_ = 0; orientation_.set_current(0);
        lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_0);
        home_rotation_pending_ = false;
        tetris_playing_ = tetris_sound_menu_ = false;
        view_ = View::Tetris; return;
    }
    if (module == ModuleId::Radar) { view_ = View::Radar; radar_page_ = 0; return; }
    if (module == ModuleId::Productivity) {
        view_ = View::Dashboard;
        if (!state_.settings.tutorial_completed) overlay_ = Overlay::Tutorial;
        return;
    }
    if (module == ModuleId::Assistant) {
        assistant_.open(static_cast<uint64_t>(esp_timer_get_time() / 1000));
        view_ = View::Assistant;
    }
    if (module == ModuleId::Hourglass) {
        hourglass_return_rotation_ = current_rotation_;
        current_rotation_ = 0;
        orientation_.set_current(0);
        lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_0);
        home_rotation_pending_ = false;
        const HourglassSnapshot snapshot = hourglass_.snapshot();
        hourglass_selected_minutes_ = snapshot.duration_minutes;
        hourglass_start_pending_ = false;
        hourglass_config_from_session_ = snapshot.state != HourglassState::Ready;
        hourglass_controls_open_ = hourglass_config_from_session_;
        view_ = hourglass_config_from_session_ ? View::Hourglass : View::HourglassSetup;
    }
}

void AppUi::set_rotation(int rotation)
{
    static const lv_display_rotation_t rotations[] = {LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90,
                                                       LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270};
    const int next = std::clamp(rotation, 0, 3);
    if (next == current_rotation_) return;
    if (home_rotation_supported_ && view_ == View::Home && overlay_ == Overlay::None && !screen_off_) {
        lv_obj_update_layout(screen_);
        const int width = lv_display_get_horizontal_resolution(display_);
        const int height = lv_display_get_vertical_resolution(display_);
        home_rotation_turns_ = shortest_rotation_turns(current_rotation_, next);
        home_rotation_sources_.clear();
        for (uint32_t i = 0; i < lv_obj_get_child_count(screen_); ++i) {
            lv_obj_t* object = lv_obj_get_child(screen_, i);
            if (object == home_sandbox_) continue;
            lv_area_t area{};
            lv_obj_get_coords(object, &area);
            home_rotation_sources_.push_back(rotate_screen_point(
                {(area.x1 + area.x2 + 1) * 0.5f, (area.y1 + area.y2 + 1) * 0.5f},
                width, height, home_rotation_turns_));
        }
        home_rotation_pending_ = true;
    }
    current_rotation_ = next;
    orientation_.set_current(current_rotation_);
    lv_display_set_rotation(display_, rotations[current_rotation_]);
    ESP_LOGI(TAG_UI, "Rotación aplicada: %d%s", next, home_rotation_pending_ ? " (Inicio animado)" : "");
    schedule_render();
}

bool AppUi::is_portrait() const
{
    return lv_display_get_horizontal_resolution(display_) < lv_display_get_vertical_resolution(display_);
}

void AppUi::set_screen_off(bool off)
{
    if (off && view_ == View::Maze) maze_.set_visible(false);
    if (off && view_ == View::Tetris) { tetris_.set_visible(false); tetris_gesture_.cancel(); }
    if (off && view_ == View::Radar) { radar_.visible(false); radar_visible_ = false; }
    if (off && (view_ == View::Hourglass || view_ == View::HourglassSetup)) {
        hourglass_.set_visible(false);
        hourglass_visible_ = false;
        hourglass_controls_open_ = true;
    }
    screen_off_ = off;
    bsp_display_brightness_set(off ? 0 : 72);
    ESP_LOGI(TAG_UI, "Pantalla %s", off ? "apagada" : "encendida");
    schedule_render();
}

void AppUi::power_off_device()
{
    ESP_LOGW(TAG_UI, "Apagando el equipo");
    hourglass_.set_visible(false);
    tetris_.set_visible(false);
    maze_.set_visible(false);
    service_.save();
    bsp_display_brightness_set(0);
    vTaskDelay(pdMS_TO_TICKS(150));
    bsp_generate_poweroff_signal();
    // Si seguimos vivos es que el equipo no se apagó (por ejemplo, alimentado por
    // USB, donde el latch solo corta la batería). Se avisa en lugar de dejar la
    // pantalla en negro sin explicación.
    set_screen_off(false);
    show_info("El equipo sigue encendido",
              "El apagado por hardware solo corta la batería. Desconecta el USB y vuelve a mantener pulsado el botón.");
}

std::string AppUi::duration_text(uint32_t seconds) const
{
    char result[24];
    std::snprintf(result, sizeof(result), "%02lu:%02lu:%02lu", static_cast<unsigned long>(seconds / 3600),
                  static_cast<unsigned long>((seconds % 3600) / 60), static_cast<unsigned long>(seconds % 60));
    return result;
}

std::string AppUi::reminder_text(const Mission& mission) const
{
    if (!mission.reminder.enabled) return "Sin recordatorio";
    if (mission.reminder.cancelled_for_pause) return "Aviso cancelado";
    if (mission.reminder.due_at_utc <= 0) return "Aviso al pausar";
    const int64_t remaining = mission.reminder.due_at_utc - clock_.now_utc();
    if (remaining <= 0) return "Recordatorio vencido";
    char result[40];
    std::snprintf(result, sizeof(result), "Aviso en %lld:%02lld", static_cast<long long>(remaining / 60),
                  static_cast<long long>(remaining % 60));
    return result;
}

std::string AppUi::truncate(const std::string& text, size_t max_chars) const
{
    if (utf8_codepoint_count(text) <= max_chars) return text;
    return utf8_truncate(text, max_chars > 3 ? max_chars - 3 : max_chars) + "...";
}

std::string AppUi::status_clock_text() const
{
    if (!clock_.is_valid()) return "--:--";
    return clock_.format_status(clock_.now_utc(), state_.settings.utc_offset_minutes);
}

std::string AppUi::status_net_text() const
{
    const ConnectivitySnapshot network = dictation_.connectivity_snapshot();
    if (network.phase == ConnectivityPhase::Ready) return "WIFI · LISTO";
    if (network.phase == ConnectivityPhase::Scanning || network.phase == ConnectivityPhase::Starting)
        return "BUSCANDO RED";
    if (network.phase == ConnectivityPhase::Connecting) return "CONECTANDO";
    if (network.phase == ConnectivityPhase::DiscoveringServer) return "BUSCANDO MAC";
    if (network.connected()) return "WIFI · " + truncate(network.current_ssid, 12);
    return connectivity_phase_name(network.phase);
}

std::string AppUi::status_power_text()
{
    const PowerStatus status = power_.poll();
    if (status.state == BatteryState::Unavailable) return "BAT --%";
    if (status.state == BatteryState::Missing) return "USB · SIN BAT";
    char output[40];
    std::snprintf(output, sizeof(output), "%sBAT ~%d%%", status.charging ? "CARGA · " : "", status.battery_pct);
    return output;
}

void AppUi::refresh_statusbar()
{
    if (status_clock_label_ == nullptr && status_net_label_ == nullptr && status_power_label_ == nullptr) return;
    const std::string clock_text = status_clock_text();
    if (clock_text != last_status_clock_ && status_clock_label_ != nullptr) {
        last_status_clock_ = clock_text;
        lv_label_set_text(status_clock_label_, clock_text.c_str());
    }
    if (clock_text != last_status_clock_ || home_time_label_ != nullptr) {
        // En Inicio, el reloj y la fecha son el primer elemento de navegación;
        // la barra superior se reserva para conectividad y batería.
        last_status_clock_ = clock_text;
        const size_t separator = clock_text.find(' ');
        if (home_time_label_ != nullptr) {
            const std::string time_text = separator == std::string::npos ? clock_text : clock_text.substr(0, separator);
            lv_label_set_text(home_time_label_, time_text.c_str());
        }
        if (home_date_label_ != nullptr) {
            const std::string date_text = separator == std::string::npos ? "" : clock_text.substr(separator + 1);
            lv_label_set_text(home_date_label_, date_text.c_str());
        }
    }
    // Estos estados se leen de snapshots; los workers realizan I2C/red.
    // Refrescar las etiquetas cada 5 s basta.
    const int64_t now = clock_.now_utc();
    if (last_status_second_ >= 0 && now - last_status_second_ < 5) return;
    last_status_second_ = now;
    const std::string net_text = status_net_text();
    if (net_text != last_status_net_ && status_net_label_ != nullptr) {
        last_status_net_ = net_text;
        lv_label_set_text(status_net_label_, net_text.c_str());
        const bool home = view_ == View::Home;
        lv_obj_set_style_text_color(status_net_label_,
                                    color(dictation_.wifi_connected() ? (home ? theme::HomeInk : CYAN)
                                                                      : (home ? theme::HomeMuted : MUTED)),
                                    LV_PART_MAIN);
    }
    const std::string power_text = status_power_text();
    if (power_text != last_status_power_ && status_power_label_ != nullptr) {
        last_status_power_ = power_text;
        lv_label_set_text(status_power_label_, power_text.c_str());
    }
}

}  // namespace el_sistema
