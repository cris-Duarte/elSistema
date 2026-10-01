#include "ui.hpp"

#include "fonts/fonts.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "esp_log.h"

#include "bsp/m5stack_tab5.h"

namespace el_sistema {
namespace {

constexpr const char* TAG_UI = "ui";

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
// El espacio entre lineas se suma al alto de linea de la fuente (35 + 16 = 51),
// de modo que cada fila supera el minimo tactil de 48.
constexpr int kClockRollerLineSpace = 16;

int clock_roller_height()
{
    return kClockVisibleRows * (lv_font_get_line_height(&font_es_28) + kClockRollerLineSpace);
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

// Teclado virtual en espanol: incluye ene, vocales acentuadas y los signos de
// apertura, de modo que no hace falta una barra de acentos aparte. Las teclas de
// control llevan texto en espanol porque las fuentes del proyecto no incluyen el
// area de simbolos FontAwesome que usa el teclado estandar de LVGL.
const char* const kKeyboardLower[] = {
    "q",     "w", "e", "r", "t", "y", "u", "i", "o",       "p",     "\n",
    "a",     "s", "d", "f", "g", "h", "j", "k", "l",       "ñ",     "\n",
    "MAYÚS", "z", "x", "c", "v", "b", "n", "m", "BORRAR",           "\n",
    "á",     "é", "í", "ó", "ú", "ü", "¿", "¡", "ESPACIO", "INTRO", "\n",
    "0",     "1", "2", "3", "4", "5", "6", "7", "8",       "9",     ""
};

const char* const kKeyboardUpper[] = {
    "Q",     "W", "E", "R", "T", "Y", "U", "I", "O",       "P",     "\n",
    "A",     "S", "D", "F", "G", "H", "J", "K", "L",       "Ñ",     "\n",
    "MAYÚS", "Z", "X", "C", "V", "B", "N", "M", "BORRAR",           "\n",
    "Á",     "É", "Í", "Ó", "Ú", "Ü", "¿", "¡", "ESPACIO", "INTRO", "\n",
    "0",     "1", "2", "3", "4", "5", "6", "7", "8",       "9",     ""
};

// Ancho en unidades y banderas por tecla. Los dos mapas comparten estructura,
// asi que sirve la misma tabla.
constexpr lv_buttonmatrix_ctrl_t key_width(int width)
{
    return static_cast<lv_buttonmatrix_ctrl_t>(width | LV_BUTTONMATRIX_CTRL_CLICK_TRIG |
                                               LV_BUTTONMATRIX_CTRL_NO_REPEAT);
}

// Una linea por fila, en el mismo orden que el mapa. Total: 10+10+9+10+10 = 49.
const lv_buttonmatrix_ctrl_t kKeyboardCtrl[] = {
    // q w e r t y u i o p
    key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5),
    key_width(5), key_width(5),
    // a s d f g h j k l ñ
    key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5),
    key_width(5), key_width(5),
    // MAYÚS z x c v b n m BORRAR
    key_width(7), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5),
    key_width(8),
    // á é í ó ú ü ¿ ¡ ESPACIO INTRO
    key_width(4), key_width(4), key_width(4), key_width(4), key_width(4), key_width(4), key_width(4), key_width(4),
    key_width(10), key_width(8),
    // 0 1 2 3 4 5 6 7 8 9
    key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5),
    key_width(5), key_width(5)
};

constexpr uint32_t kShiftButton = 20;  // indice de "MAYÚS" dentro del mapa

}  // namespace

AppUi::AppUi(AppState& state, ApplicationService& service, MissionRepository& repository, ClockService& clock,
             OrientationService& orientation, ButtonService& button)
    : state_(state), service_(service), repository_(repository), clock_(clock), orientation_(orientation),
      button_(button)
{
}

void AppUi::start(lv_display_t* display)
{
    display_ = display;
    screen_ = lv_screen_active();
    current_rotation_ = 1;
    orientation_.set_current(current_rotation_);
    if (!clock_.is_valid()) {
        seed_clock_defaults();
        view_ = View::ClockSetup;
    } else if (!state_.settings.tutorial_completed) {
        overlay_ = Overlay::Tutorial;
    }
    render();
    tick_timer_ = lv_timer_create(tick_callback, 250, this);
}

void AppUi::action_callback(lv_event_t* event)
{
    auto* action = static_cast<Action*>(lv_event_get_user_data(event));
    if (action != nullptr && action->ui != nullptr) action->ui->dispatch(*action);
}

void AppUi::tick_callback(lv_timer_t* timer)
{
    auto* ui = static_cast<AppUi*>(lv_timer_get_user_data(timer));
    if (ui != nullptr) ui->tick();
}

void AppUi::render_callback(void* user_data)
{
    static_cast<AppUi*>(user_data)->render();
}

void AppUi::schedule_render()
{
    lv_async_call(render_callback, this);
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
    lv_obj_t* caption = label(object, text_value, &font_es_16, TEXT);
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
    lv_obj_clean(screen_);
    actions_.clear();
    reminder_labels_.clear();
    timer_label_ = nullptr;
    active_status_label_ = nullptr;
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

    lv_obj_set_style_bg_color(screen_, color(BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen_, &font_es_16, LV_PART_MAIN);

    // Con la pantalla apagada solo se dibuja la capa que absorbe el tacto, para
    // que un roce no active botones invisibles. La vista se reconstruye al volver.
    if (screen_off_) {
        render_screen_off();
        return;
    }

    switch (view_) {
        case View::Dashboard: render_dashboard(); break;
        case View::History: render_history(); break;
        case View::Editor: render_editor(false); break;
        case View::SearchEditor: render_editor(true); break;
        case View::ClockSetup: render_clock_setup(); break;
    }
    if (overlay_ != Overlay::None) render_overlay();
}

void AppUi::render_header(bool compact)
{
    const int width = lv_display_get_horizontal_resolution(display_);
    lv_obj_t* header = panel(screen_, 0, 0, width, compact ? 78 : 90, false);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_bg_color(header, color(PANEL), LV_PART_MAIN);

    lv_obj_t* title = label(header, "EL SISTEMA", &font_es_28, TEXT);
    lv_obj_set_pos(title, 20, compact ? 14 : 20);
    lv_obj_t* subtitle = label(header, "MÓDULO DE PRODUCTIVIDAD", &font_es_16, CYAN);
    lv_obj_set_pos(subtitle, 22, compact ? 45 : 57);

    const uint32_t required = XpService::required_for_level(state_.profile.level);
    char profile[96];
    std::snprintf(profile, sizeof(profile), "NIVEL %lu   %lu / %lu XP", static_cast<unsigned long>(state_.profile.level),
                  static_cast<unsigned long>(state_.profile.level_xp), static_cast<unsigned long>(required));
    lv_obj_t* xp = label(header, profile, &font_es_16, CYAN);
    lv_obj_set_pos(xp, std::max(280, width / 2 - 130), compact ? 26 : 34);

    if (repository_.storage_percent() >= 80) {
        char storage[48];
        std::snprintf(storage, sizeof(storage), "ALMACENAMIENTO %u%%", repository_.storage_percent());
        lv_obj_t* warning = label(header, storage, &font_es_16, DANGER);
        lv_obj_set_pos(warning, width - 520, 8);
    }

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

void AppUi::render_landscape_dashboard()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    const int top = 102;
    const int gap = 14;
    const int left = static_cast<int>(width * 0.60f);
    // La geometria se calcula aqui en vez de preguntarsela a los widgets: en LVGL 9
    // lv_obj_get_width/height devuelven obj->coords, que solo se rellena en la pasada
    // de layout, asi que justo despues de crearlos valdria 0. Con ese 0 las filas de
    // las listas salian con ancho negativo (invisibles) y el panel de backlog se
    // colocaba encima del de enfoque: la columna derecha quedaba vacia.
    const int right_x = left + gap;
    const int right_width = width - right_x - 18;
    const int panel_height = (height - top - 30) / 2;

    lv_obj_t* active = panel(screen_, 18, top, left - 25, height - top - 16, false);
    render_active_panel(active, left - 25, height - top - 16);

    lv_obj_t* focus = panel(screen_, right_x, top, right_width, panel_height, true);
    render_focus_list(focus, right_width, panel_height);

    lv_obj_t* backlog = panel(screen_, right_x, top + panel_height + gap, right_width, panel_height, true);
    render_backlog_list(backlog, right_width, panel_height);
}

void AppUi::render_portrait_dashboard()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    lv_obj_t* active = panel(screen_, 16, 92, width - 32, 405, false);
    render_active_panel(active, width - 32, 405);
    button(screen_, "ZONA DE ENFOQUE", 16, 510, (width - 44) / 2, 54, ActionCode::CycleDateFilter, 0, -10,
           nullptr, portrait_tab_ == 0 ? ACCENT_SOFT : PANEL_ALT, portrait_tab_ == 0 ? CYAN : LINE);
    button(screen_, "BACKLOG", 28 + (width - 44) / 2, 510, (width - 44) / 2, 54, ActionCode::CycleDateFilter, 0,
           -11, nullptr, portrait_tab_ == 1 ? ACCENT_SOFT : PANEL_ALT, portrait_tab_ == 1 ? CYAN : LINE);
    lv_obj_t* list = panel(screen_, 16, 576, width - 32, height - 592, true);
    if (portrait_tab_ == 0) render_focus_list(list, width - 32, height - 592);
    else render_backlog_list(list, width - 32, height - 592);
}

void AppUi::render_active_panel(lv_obj_t* parent, int width, int height)
{
    lv_obj_t* heading = label(parent, "MISIÓN ACTIVA", &font_es_16, CYAN);
    lv_obj_set_pos(heading, 22, 18);
    const Mission* mission = find_mission(state_, state_.focus.active_id);
    if (mission == nullptr) {
        lv_obj_t* empty = label(parent, "NINGUNA MISIÓN EN CURSO", &font_es_28, MUTED);
        lv_obj_set_width(empty, width - 60);
        lv_obj_set_pos(empty, 28, 86);
        lv_obj_t* help = label(parent,
                               "Elige una misión del backlog o de la Zona de Enfoque.\nSolo un cronómetro puede estar activo.",
                               &font_es_16, TEXT);
        lv_obj_set_width(help, width - 60);
        lv_obj_set_pos(help, 30, 142);
        button(parent, "+ CREAR MISIÓN", 30, height - 78, 180, 50, ActionCode::NewMission, 0, 0, nullptr,
               ACCENT_SOFT, CYAN);
        button(parent, "MICRÓFONO", 222, height - 78, 150, 50, ActionCode::DictationInfo);
        return;
    }

    char badge[16];
    std::snprintf(badge, sizeof(badge), "RANGO %s", rank_to_string(mission->rank));
    lv_obj_t* rank = label(parent, badge, &font_es_16, rank_color(mission->rank));
    lv_obj_set_pos(rank, 22, 54);
    lv_obj_t* title = label(parent, mission->title.c_str(), &font_es_28, TEXT);
    lv_obj_set_width(title, width - 54);
    lv_obj_set_pos(title, 22, 88);
    if (!mission->description.empty()) {
        lv_obj_t* description = label(parent, mission->description.c_str(), &font_es_16, MUTED);
        lv_obj_set_width(description, width - 54);
        lv_obj_set_pos(description, 24, 140);
    }
    timer_label_ = label(parent, duration_text(service_.current_elapsed(*mission)).c_str(), &font_es_28, CYAN);
    lv_obj_set_pos(timer_label_, 24, std::max(198, height / 2 - 24));
    active_status_label_ = label(parent, "CRONÓMETRO ACTIVO", &font_es_16, CYAN);
    lv_obj_set_pos(active_status_label_, 26, std::max(236, height / 2 + 22));

    const int y = height - 76;
    button(parent, "PAUSAR", 22, y, 120, 48, ActionCode::PauseMission, mission->id, 0, nullptr, PANEL_ALT, CYAN);
    button(parent, "EDITAR", 154, y, 120, 48, ActionCode::EditMission, mission->id);
    button(parent, "COMPLETAR", 286, y, 152, 48, ActionCode::CompleteMission, mission->id, 0, nullptr, ACCENT_SOFT,
           CYAN);
    button(parent, "MIC", width - 102, y, 76, 48, ActionCode::DictationInfo);
}

void AppUi::render_focus_list(lv_obj_t* parent, int width, int)
{
    lv_obj_t* heading = label(parent, "ZONA DE ENFOQUE", &font_es_16, VIOLET);
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
        lv_obj_t* empty = label(parent, "La cola está vacía.", &font_es_16, MUTED);
        lv_obj_set_pos(empty, 18, 61);
    }
}

void AppUi::render_backlog_list(lv_obj_t* parent, int width, int)
{
    lv_obj_t* heading = label(parent, "BACKLOG", &font_es_16, CYAN);
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
        lv_obj_t* empty = label(parent, "No hay misiones pendientes.", &font_es_16, MUTED);
        lv_obj_set_pos(empty, 18, 61);
    }
}

void AppUi::render_mission_row(lv_obj_t* parent, const Mission& mission, bool focus, int width)
{
    lv_obj_t* rank = label(parent, rank_to_string(mission.rank), &font_es_20, rank_color(mission.rank));
    lv_obj_set_pos(rank, 12, 12);
    lv_obj_t* title = label(parent, truncate(mission.title, is_portrait() ? 50 : 32).c_str(), &font_es_16, TEXT);
    lv_obj_set_width(title, width - 80);
    lv_obj_set_pos(title, 42, 11);
    lv_obj_t* elapsed = label(parent, duration_text(service_.current_elapsed(mission)).c_str(), &font_es_16, MUTED);
    lv_obj_set_pos(elapsed, 42, 39);
    if (focus) {
        lv_obj_t* reminder = label(parent, reminder_text(mission).c_str(), &font_es_16, CYAN);
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
    button(screen_, "VOLVER", 18, 102, 110, 48, ActionCode::CloseHistory);
    button(screen_, search_query_.empty() ? "BUSCAR" : truncate(search_query_, 18).c_str(), 140, 102,
           is_portrait() ? 210 : 300, 48, ActionCode::OpenSearch);
    const char* rank_text = rank_filter_ < 0 ? "RANGO: TODOS" : rank_to_string(rank_from_index(rank_filter_));
    button(screen_, rank_text, is_portrait() ? 18 : 452, is_portrait() ? 160 : 102, 150, 48,
           ActionCode::CycleRankFilter);
    const char* date_text = date_filter_days_ == 0 ? "FECHA: TODAS" : date_filter_days_ == 30 ? "ÚLTIMOS 30 DÍAS"
                                                       : date_filter_days_ == 90 ? "ÚLTIMOS 90 DÍAS"
                                                                                 : "ÚLTIMO AÑO";
    button(screen_, date_text, is_portrait() ? 180 : 614, is_portrait() ? 160 : 102, 174, 48,
           ActionCode::CycleDateFilter);
    button(screen_, newest_first_ ? "MÁS RECIENTES" : "MÁS ANTIGUAS", is_portrait() ? 366 : 800,
           is_portrait() ? 160 : 102, 174, 48, ActionCode::ToggleOrder);

    const int list_y = is_portrait() ? 222 : 164;
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
        lv_obj_t* rank = label(row, rank_to_string(mission->rank), &font_es_20, rank_color(mission->rank));
        lv_obj_set_pos(rank, 14, 15);
        lv_obj_t* title = label(row, mission->title.c_str(), &font_es_16, TEXT);
        lv_obj_set_width(title, width - 370);
        lv_obj_set_pos(title, 48, 13);
        std::string meta = clock_.format_local(mission->completed_at_utc, state_.settings.utc_offset_minutes) +
                           "   " + duration_text(mission->elapsed_seconds) + "   +" +
                           std::to_string(mission->xp_awarded) + " XP";
        lv_obj_t* detail = label(row, meta.c_str(), &font_es_16, MUTED);
        lv_obj_set_pos(detail, 48, 43);
        button(row, "DETALLE", width - 276, 24, 92, 48, ActionCode::ShowDetail, mission->id);
        button(row, "ELIMINAR", width - 174, 24, 92, 48, ActionCode::AskDelete, mission->id, 0, nullptr,
               DANGER_SOFT, DANGER);
        y += row_height + 12;
    }
    if (completed.empty()) {
        lv_obj_t* empty = label(list, "No hay misiones completadas para estos filtros.", &font_es_20, MUTED);
        lv_obj_set_pos(empty, 24, 32);
    }
}

void AppUi::render_editor(bool search_only)
{
    const int width = lv_display_get_horizontal_resolution(display_);
    const int height = lv_display_get_vertical_resolution(display_);
    lv_obj_t* title = label(screen_, search_only ? "BUSCAR EN HISTORIAL" : editing_id_ ? "EDITAR MISIÓN" : "NUEVA MISIÓN",
                            &font_es_28, TEXT);
    lv_obj_set_pos(title, 20, 18);
    button(screen_, "CANCELAR", width - 134, 14, 116, 48, ActionCode::CancelEditor);

    const int form_width = width - 40;
    title_textarea_ = lv_textarea_create(screen_);
    place(title_textarea_, 20, 78, form_width, search_only ? 76 : 68);
    base_style(title_textarea_, PANEL_ALT, LINE);
    lv_obj_set_style_text_color(title_textarea_, color(TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_textarea_, &font_es_16, LV_PART_MAIN);
    lv_textarea_set_placeholder_text(title_textarea_, search_only ? "Título o descripción" : "Título de la misión");
    lv_textarea_set_max_length(title_textarea_, search_only ? 80 : 80);
    lv_textarea_set_one_line(title_textarea_, true);
    lv_textarea_set_text(title_textarea_, search_only ? search_query_.c_str() : editor_title_.c_str());
    lv_obj_add_event_cb(title_textarea_, action_callback, LV_EVENT_FOCUSED, make_action(ActionCode::FocusTitle));

    int controls_bottom = 162;
    if (!search_only) {
        description_textarea_ = lv_textarea_create(screen_);
        place(description_textarea_, 20, 158, form_width, 92);
        base_style(description_textarea_, PANEL_ALT, LINE);
        lv_obj_set_style_text_color(description_textarea_, color(TEXT), LV_PART_MAIN);
        lv_obj_set_style_text_font(description_textarea_, &font_es_16, LV_PART_MAIN);
        lv_textarea_set_placeholder_text(description_textarea_, "Descripción opcional");
        lv_textarea_set_max_length(description_textarea_, 240);
        lv_textarea_set_text(description_textarea_, editor_description_.c_str());
        lv_obj_add_event_cb(description_textarea_, action_callback, LV_EVENT_FOCUSED,
                            make_action(ActionCode::FocusDescription));
        lv_obj_t* rank_caption = label(screen_, "RANGO", &font_es_16, CYAN);
        lv_obj_set_pos(rank_caption, 22, 266);
        for (int index = 0; index < 6; ++index) {
            const MissionRank rank = rank_from_index(index);
            button(screen_, rank_to_string(rank), 92 + index * 58, 258, 48, 48, ActionCode::SelectRank, 0, index,
                   nullptr, editor_rank_ == rank ? ACCENT_SOFT : PANEL_ALT,
                   editor_rank_ == rank ? rank_color(rank) : LINE);
        }
        lv_obj_t* reminder_caption = label(screen_, "RECORDATORIO", &font_es_16, CYAN);
        lv_obj_set_pos(reminder_caption, 470, 266);
        button(screen_, editor_reminder_enabled_ ? "ACTIVO" : "APAGADO", 610, 258, 100, 48,
               ActionCode::ReminderToggle, 0, 0, nullptr, editor_reminder_enabled_ ? ACCENT_SOFT : PANEL_ALT,
               editor_reminder_enabled_ ? CYAN : LINE);
        button(screen_, "-", 722, 258, 46, 48, ActionCode::ReminderMinus);
        char minutes[32];
        std::snprintf(minutes, sizeof(minutes), "%u MIN", editor_reminder_minutes_);
        reminder_value_label_ = label(screen_, minutes, &font_es_16, TEXT);
        lv_obj_set_pos(reminder_value_label_, 780, 272);
        button(screen_, "+", 860, 258, 46, 48, ActionCode::ReminderPlus);
        controls_bottom = 316;
    }

    // El teclado en espanol lleva los acentos incorporados, asi que sustituye a la
    // antigua barra de acentos y aprovecha su espacio para teclas mas grandes.
    const int keyboard_y = controls_bottom + 10;
    const int available = height - keyboard_y - 72;
    keyboard_ = spanish_keyboard(20, keyboard_y, width - 40, std::min(360, available));

    button(screen_, "MICRÓFONO", 20, height - 62, 150, 48, ActionCode::DictationInfo);
    button(screen_, search_only ? "APLICAR BÚSQUEDA" : "GUARDAR MISIÓN", width - 216, height - 62, 198, 48,
           search_only ? ActionCode::ApplySearch : ActionCode::SaveMission, 0, 0, nullptr, ACCENT_SOFT, CYAN);

    set_active_textarea(title_textarea_);
}

lv_obj_t* AppUi::spanish_keyboard(int x, int y, int width, int height)
{
    lv_obj_t* matrix = lv_buttonmatrix_create(screen_);
    place(matrix, x, y, width, height);
    lv_buttonmatrix_set_map(matrix, keyboard_upper_ ? kKeyboardUpper : kKeyboardLower);
    lv_buttonmatrix_set_ctrl_map(matrix, kKeyboardCtrl);
    lv_obj_add_event_cb(matrix, action_callback, LV_EVENT_VALUE_CHANGED, make_action(ActionCode::KeyboardKey));

    lv_obj_set_style_bg_color(matrix, color(PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(matrix, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(matrix, theme::RadiusPanel, LV_PART_MAIN);
    lv_obj_set_style_pad_all(matrix, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(matrix, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(matrix, 6, LV_PART_MAIN);

    lv_obj_set_style_bg_color(matrix, color(PANEL_ALT), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(matrix, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_color(matrix, color(LINE), LV_PART_ITEMS);
    lv_obj_set_style_border_width(matrix, theme::BorderWidth, LV_PART_ITEMS);
    lv_obj_set_style_radius(matrix, theme::RadiusControl, LV_PART_ITEMS);
    lv_obj_set_style_text_font(matrix, &font_es_20, LV_PART_ITEMS);
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
    return matrix;
}

void AppUi::handle_key(const char* text)
{
    if (text == nullptr || active_textarea_ == nullptr) return;
    if (std::strcmp(text, "BORRAR") == 0) {
        lv_textarea_delete_char(active_textarea_);
        return;
    }
    if (std::strcmp(text, "ESPACIO") == 0) {
        lv_textarea_add_text(active_textarea_, " ");
        return;
    }
    if (std::strcmp(text, "INTRO") == 0) {
        // Las areas de una linea ignoran el salto (el titulo); la descripcion lo usa.
        lv_textarea_add_text(active_textarea_, "\n");
        return;
    }
    if (std::strcmp(text, "MAYÚS") == 0) {
        keyboard_upper_ = !keyboard_upper_;
        lv_buttonmatrix_set_map(keyboard_, keyboard_upper_ ? kKeyboardUpper : kKeyboardLower);
        lv_buttonmatrix_set_ctrl_map(keyboard_, kKeyboardCtrl);
        // El mapa de control se reaplica entero, asi que el resaltado va despues.
        if (keyboard_upper_) {
            lv_buttonmatrix_set_button_ctrl(keyboard_, kShiftButton, LV_BUTTONMATRIX_CTRL_CHECKED);
        } else {
            lv_buttonmatrix_clear_button_ctrl(keyboard_, kShiftButton, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
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

    lv_obj_t* title = label(screen_, "CONFIGURAR FECHA Y HORA", &font_es_28, TEXT);
    lv_obj_set_pos(title, 30, 24);
    const std::string help_text = "Desliza cada columna para ajustar. Zona: " + state_.settings.timezone + ".";
    lv_obj_t* help = label(screen_, help_text.c_str(), &font_es_16, MUTED);
    lv_obj_set_pos(help, 32, 68);
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

    constexpr int kCaptionY = 120;
    constexpr int kRollerY = 152;
    for (int index = 0; index < 5; ++index) {
        lv_obj_t* caption = label(screen_, kCaptions[index], &font_es_16, CYAN);
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

    lv_obj_t* zone = label(screen_, "ZONA HORARIA", &font_es_16, CYAN);
    lv_obj_set_pos(zone, 32, rollers_bottom + 38);

    char offset[32];
    const int hours = state_.settings.utc_offset_minutes / 60;
    const int minutes = std::abs(state_.settings.utc_offset_minutes % 60);
    std::snprintf(offset, sizeof(offset), "UTC%+d:%02d", hours, minutes);
    button(screen_, "-", 30, action_y, 64, 64, ActionCode::ClockOffsetMinus);
    lv_obj_t* offset_label = label(screen_, offset, &font_es_20, CYAN);
    lv_obj_set_pos(offset_label, 114, action_y + 21);
    button(screen_, "+", 246, action_y, 64, 64, ActionCode::ClockOffsetPlus);
    lv_obj_t* step_hint = label(screen_, "Pasos de 30 min", &font_es_16, MUTED);
    lv_obj_set_pos(step_hint, 330, action_y + 25);
    button(screen_, "GUARDAR Y CONTINUAR", width - 372, action_y, 340, 64, ActionCode::SaveClock, 0, 0, nullptr,
           ACCENT_SOFT, CYAN);

    lv_obj_t* note = label(screen_,
                           "Valor inicial: fecha de compilación del firmware. La hora solo fecha los intervalos; el "
                           "cronómetro mide con el reloj monotónico.",
                           &font_es_16, MUTED);
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
    lv_obj_set_style_text_font(roller, &font_es_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, color(MUTED), LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, color(ACCENT_SOFT), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &font_es_28, LV_PART_SELECTED);
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
    const int box_height = overlay_ == Overlay::MissionDetail ? std::min(height - 80, 510) : 360;
    lv_obj_t* box = panel(shade, (width - box_width) / 2, (height - box_height) / 2, box_width, box_height, true);

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

    lv_obj_t* heading = label(box, title_text.c_str(), &font_es_28,
                              overlay_ == Overlay::Reward ? GOLD : CYAN);
    lv_obj_set_width(heading, box_width - 48);
    lv_obj_set_pos(heading, 24, 24);
    if (overlay_ == Overlay::Reward) {
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, heading);
        lv_anim_set_values(&animation, 180, 256);
        lv_anim_set_duration(&animation, 420);
        lv_anim_set_playback_duration(&animation, 180);
        lv_anim_set_exec_cb(&animation, [](void* object, int32_t value) {
            lv_obj_set_style_transform_scale(static_cast<lv_obj_t*>(object), value, LV_PART_MAIN);
        });
        lv_anim_start(&animation);
    }
    lv_obj_t* body = label(box, body_text.c_str(), &font_es_16, TEXT);
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
            if (service_.complete_active(xp, next)) {
                overlay_title_ = "MISIÓN COMPLETADA";
                overlay_body_ = "+" + std::to_string(xp) + " XP obtenida\nNivel " +
                                std::to_string(state_.profile.level);
                suggested_next_id_ = next;
                overlay_ = Overlay::Reward;
            }
            break;
        }
        case ActionCode::RemoveFocus: service_.remove_from_focus(action.id); break;
        case ActionCode::MoveUp: service_.move_in_focus(action.id, -1); break;
        case ActionCode::MoveDown: service_.move_in_focus(action.id, 1); break;
        case ActionCode::OpenHistory:
            view_ = View::History;
            break;
        case ActionCode::CloseHistory:
            view_ = View::Dashboard;
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
            break;
        case ActionCode::StartSuggested:
            service_.start(action.id);
            overlay_ = Overlay::None;
            break;
        case ActionCode::ReminderResume:
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
        case ActionCode::DictationInfo:
            show_info("Dictado no configurado", "El botón permanecerá disponible. El dictado privado llegará en v0.4.0; mientras tanto usa el teclado táctil.");
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

    static int64_t last_second = -1;
    const int64_t now_second = clock_.now_utc();
    if (now_second != last_second) {
        last_second = now_second;
        const Mission* active = find_mission(state_, state_.focus.active_id);
        if (active != nullptr && timer_label_ != nullptr) {
            lv_label_set_text(timer_label_, duration_text(service_.current_elapsed(*active)).c_str());
            const int checkpoint_slot = static_cast<int>(service_.current_elapsed(*active) / 30);
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
        if (overlay_ == Overlay::None) {
            const std::vector<MissionId> due = service_.due_reminders();
            if (!due.empty()) {
                overlay_mission_id_ = due.front();
                overlay_ = Overlay::Reminder;
                schedule_render();
                return;
            }
        }
    }

    const bool rotation_locked = view_ == View::Editor || view_ == View::SearchEditor || view_ == View::ClockSetup ||
                                 overlay_ != Overlay::None || screen_off_;
    if (!rotation_locked) {
        const int next = orientation_.poll();
        if (next >= 0 && next != current_rotation_) set_rotation(next);
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
    view_ = View::Dashboard;
    if (!state_.settings.tutorial_completed) overlay_ = Overlay::Tutorial;
}

void AppUi::set_rotation(int rotation)
{
    static const lv_display_rotation_t rotations[] = {LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90,
                                                       LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270};
    current_rotation_ = std::clamp(rotation, 0, 3);
    orientation_.set_current(current_rotation_);
    lv_display_set_rotation(display_, rotations[current_rotation_]);
    schedule_render();
}

bool AppUi::is_portrait() const
{
    return lv_display_get_horizontal_resolution(display_) < lv_display_get_vertical_resolution(display_);
}

void AppUi::set_screen_off(bool off)
{
    screen_off_ = off;
    bsp_display_brightness_set(off ? 0 : 72);
    ESP_LOGI(TAG_UI, "Pantalla %s", off ? "apagada" : "encendida");
    schedule_render();
}

void AppUi::power_off_device()
{
    ESP_LOGW(TAG_UI, "Apagando el equipo");
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
    if (text.size() <= max_chars) return text;
    return text.substr(0, max_chars > 3 ? max_chars - 3 : max_chars) + "...";
}

}  // namespace el_sistema
