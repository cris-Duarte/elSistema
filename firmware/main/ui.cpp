#include "ui.hpp"

#include "assets/assistant_character.h"
#include "fonts/fonts.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "esp_log.h"
#include "esp_timer.h"

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

// Alias de fuente (main/fonts/fonts.h). Igual que con el color, un solo punto de
// definicion: cambiar de fuente es cambiar estas cuatro lineas.
const lv_font_t& FONT_BODY = font_poppins_16;      // contenido y texto corrido
const lv_font_t& FONT_LABEL = font_rajdhani_20;    // rotulos, botones, teclas
const lv_font_t& FONT_DISPLAY = font_rajdhani_28;  // titulos, mision activa, ruedas
const lv_font_t& FONT_NUMBER = font_rajdhani_40;   // titulo protagonista, nivel
const lv_font_t& FONT_HERO = font_rajdhani_128;    // cronometro de la mision activa

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

// Teclado virtual en espanol: incluye ene, vocales acentuadas y los signos de
// apertura, de modo que no hace falta una barra de acentos aparte. Las teclas de
// control llevan texto en espanol porque las fuentes del proyecto no incluyen el
// area de simbolos FontAwesome que usa el teclado estandar de LVGL.
const char* const kKeyboardLower[] = {
    "q",     "w", "e", "r", "t", "y", "u", "i", "o",       "p",     "\n",
    "a",     "s", "d", "f", "g", "h", "j", "k", "l",       "ñ",     "\n",
    "MAYÚS", "z", "x", "c", "v", "b", "n", "m", "BORRAR",           "\n",
    "á",     "é", "í", "ó", "ú", "ü", "¿", "¡", "ESPACIO", "INTRO", "\n",
    "0",     "1", "2", "3", "4", "5", "6", "7", "8",       "9",     "\n",
    ".",     "-", "_", "@", "#", "$", "%", "&", "*",       "!",     ""
};

const char* const kKeyboardUpper[] = {
    "Q",     "W", "E", "R", "T", "Y", "U", "I", "O",       "P",     "\n",
    "A",     "S", "D", "F", "G", "H", "J", "K", "L",       "Ñ",     "\n",
    "MAYÚS", "Z", "X", "C", "V", "B", "N", "M", "BORRAR",           "\n",
    "Á",     "É", "Í", "Ó", "Ú", "Ü", "¿", "¡", "ESPACIO", "INTRO", "\n",
    "0",     "1", "2", "3", "4", "5", "6", "7", "8",       "9",     "\n",
    ".",     "-", "_", "@", "#", "$", "%", "&", "*",       "!",     ""
};

// Ancho en unidades y banderas por tecla. Los dos mapas comparten estructura,
// asi que sirve la misma tabla.
constexpr lv_buttonmatrix_ctrl_t key_width(int width)
{
    return static_cast<lv_buttonmatrix_ctrl_t>(width | LV_BUTTONMATRIX_CTRL_CLICK_TRIG |
                                               LV_BUTTONMATRIX_CTRL_NO_REPEAT);
}

// Una linea por fila, en el mismo orden que el mapa. La ultima fila permite
// escribir contraseñas e IP sin depender de otro teclado.
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
    key_width(5), key_width(5),
    // . - _ @ # $ % & * !
    key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5), key_width(5),
    key_width(5), key_width(5)
};

constexpr uint32_t kShiftButton = 20;  // indice de "MAYÚS" dentro del mapa

// Barra superior de estado: la parte visual sigue siendo fina, pero el estado
// Wi-Fi tiene una zona táctil transparente de 44 px.
// Se dibuja la última para que ningún panel ni diálogo la tape.
constexpr int kStatusBarHeight = 22;

}  // namespace

AppUi::AppUi(AppState& state, ApplicationService& service, MissionRepository& repository, ClockService& clock,
             OrientationService& orientation, ButtonService& button, DictationService& dictation,
             PowerService& power, AssistantService& assistant, AssistantVoiceService& assistant_voice)
    : state_(state), service_(service), repository_(repository), clock_(clock), orientation_(orientation),
      button_(button), dictation_(dictation), power_(power), assistant_(assistant), assistant_voice_(assistant_voice)
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
    lv_obj_clean(screen_);
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

    lv_obj_set_style_bg_color(screen_, color(BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_font(screen_, &FONT_BODY, LV_PART_MAIN);

    // Con la pantalla apagada solo se dibuja la capa que absorbe el tacto, para
    // que un roce no active botones invisibles. La vista se reconstruye al volver.
    if (screen_off_) {
        render_screen_off();
        return;
    }

    switch (view_) {
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
    render_statusbar();
}

void AppUi::render_statusbar()
{
    const int width = lv_display_get_horizontal_resolution(display_);
    lv_obj_t* bar = lv_obj_create(screen_);
    place(bar, 0, 0, width, kStatusBarHeight);
    lv_obj_set_style_bg_color(bar, color(PANEL), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, color(LINE), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    last_status_clock_ = status_clock_text();
    last_status_net_ = status_net_text();
    last_status_power_ = status_power_text();
    last_status_second_ = clock_.now_utc();

    status_clock_label_ = label(screen_, last_status_clock_.c_str(), &FONT_BODY, TEXT);
    lv_obj_set_pos(status_clock_label_, 20, 1);
    status_net_label_ = label(screen_, last_status_net_.c_str(), &FONT_BODY,
                              dictation_.wifi_connected() ? CYAN : MUTED);
    lv_obj_set_width(status_net_label_, 220);
    lv_obj_set_style_text_align(status_net_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(status_net_label_, width - 560, 1);
    lv_obj_t* network_touch = lv_obj_create(screen_);
    place(network_touch, width - 560, 0, 220, 44);
    lv_obj_set_style_bg_opa(network_touch, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(network_touch, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(network_touch, 0, LV_PART_MAIN);
    lv_obj_remove_flag(network_touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(network_touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(network_touch, action_callback, LV_EVENT_CLICKED, make_action(ActionCode::OpenConnectivity));
    status_power_label_ = label(screen_, last_status_power_.c_str(), &FONT_BODY, MUTED);
    lv_obj_set_width(status_power_label_, 320);
    lv_obj_set_style_text_align(status_power_label_, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(status_power_label_, width - 340, 1);
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

    button(header, "ASISTENTE", width - 430, 17, 126, 50, ActionCode::OpenAssistant, 0, 0, nullptr,
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
    button(header, "PRODUCTIVIDAD", width - 150, 14, 132, 50, ActionCode::CloseAssistant);

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

    int controls_bottom = 162 + kStatusBarHeight;
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
        lv_obj_t* rank_caption = label(screen_, "RANGO", &FONT_LABEL, CYAN);
        lv_obj_set_pos(rank_caption, 22, 266 + kStatusBarHeight);
        for (int index = 0; index < 6; ++index) {
            const MissionRank rank = rank_from_index(index);
            button(screen_, rank_to_string(rank), 92 + index * 58, 258 + kStatusBarHeight, 48, 48,
                   ActionCode::SelectRank, 0, index, nullptr, editor_rank_ == rank ? ACCENT_SOFT : PANEL_ALT,
                   editor_rank_ == rank ? rank_color(rank) : LINE);
        }
        lv_obj_t* reminder_caption = label(screen_, "RECORDATORIO", &FONT_LABEL, CYAN);
        lv_obj_set_pos(reminder_caption, 470, 266 + kStatusBarHeight);
        button(screen_, editor_reminder_enabled_ ? "ACTIVO" : "APAGADO", 610, 258 + kStatusBarHeight, 100, 48,
               ActionCode::ReminderToggle, 0, 0, nullptr, editor_reminder_enabled_ ? ACCENT_SOFT : PANEL_ALT,
               editor_reminder_enabled_ ? CYAN : LINE);
        button(screen_, "-", 722, 258 + kStatusBarHeight, 46, 48, ActionCode::ReminderMinus);
        char minutes[32];
        std::snprintf(minutes, sizeof(minutes), "%u MIN", editor_reminder_minutes_);
        reminder_value_label_ = label(screen_, minutes, &FONT_LABEL, TEXT);
        lv_obj_set_pos(reminder_value_label_, 780, 272 + kStatusBarHeight);
        button(screen_, "+", 860, 258 + kStatusBarHeight, 46, 48, ActionCode::ReminderPlus);
        controls_bottom = 316 + kStatusBarHeight;
    }

    // El teclado en espanol lleva los acentos incorporados, asi que sustituye a la
    // antigua barra de acentos y aprovecha su espacio para teclas mas grandes.
    const int keyboard_y = controls_bottom + 10;
    const int available = height - keyboard_y - 72;
    keyboard_ = spanish_keyboard(20, keyboard_y, width - 40, std::min(360, available));

    button(screen_, "MICRÓFONO", 20, height - 62, 150, 48, ActionCode::OpenDictation);
    button(screen_, search_only ? "APLICAR BÚSQUEDA" : "GUARDAR MISIÓN", width - 216, height - 62, 198, 48,
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
        const int keyboard_height = std::min(330, std::max(230, height / 2 - 24));
        const int keyboard_y = height - keyboard_height - 10;
        lv_obj_t* form = panel(screen_, 18, 76 + kStatusBarHeight, width - 36,
                               keyboard_y - 86 - kStatusBarHeight, false);
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
        int button_y = 114;
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
            button_y = 214;
        }
        button(form, network_connect_pending_ ? "CONECTANDO…" : "CONECTAR Y GUARDAR", 24, button_y,
               std::min(260, field_width), 52,
               ActionCode::ConnectAndSaveNetwork, 0, 0, nullptr, ACCENT_SOFT, CYAN);
        const std::string form_note = network_connect_pending_
            ? "Comprobando la red; todavía no se guardó la contraseña."
            : snapshot.error.empty() ? "La red solo se guardará después de obtener conexión." : snapshot.error;
        lv_obj_t* note = label(form, form_note.c_str(), &FONT_BODY, snapshot.error.empty() ? MUTED : DANGER);
        lv_obj_set_pos(note, 304, button_y + 14);
        lv_obj_set_width(note, std::max(200, field_width - 300));
        keyboard_ = spanish_keyboard(18, keyboard_y, width - 36, keyboard_height);
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
    lv_obj_set_style_text_font(matrix, &FONT_LABEL, LV_PART_ITEMS);
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
            overlay_ = Overlay::None;
            assistant_.open(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            view_ = View::Assistant;
            break;
        case ActionCode::CloseAssistant:
            assistant_voice_.cancel();
            assistant_.close(static_cast<uint64_t>(esp_timer_get_time() / 1000));
            view_ = View::Dashboard;
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
        if (overlay_ == Overlay::None && !dictation_.busy() && !assistant_voice_.busy()) {
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
                                 view_ == View::DictationSetup || overlay_ != Overlay::None || screen_off_ ||
                                 dictation_.busy() || assistant_voice_.busy();
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
    // Sin batería el equipo solo puede estar corriendo por USB.
    if (status.battery_pct < 0) return "USB";
    char output[40];
    std::snprintf(output, sizeof(output), "%sBAT %d%%", status.charging ? "USB · " : "", status.battery_pct);
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
    // La red y la energía implican lecturas I2C: cada 5 s basta.
    const int64_t now = clock_.now_utc();
    if (last_status_second_ >= 0 && now - last_status_second_ < 5) return;
    last_status_second_ = now;
    const std::string net_text = status_net_text();
    if (net_text != last_status_net_ && status_net_label_ != nullptr) {
        last_status_net_ = net_text;
        lv_label_set_text(status_net_label_, net_text.c_str());
        lv_obj_set_style_text_color(status_net_label_, color(dictation_.wifi_connected() ? CYAN : MUTED),
                                    LV_PART_MAIN);
    }
    const std::string power_text = status_power_text();
    if (power_text != last_status_power_ && status_power_label_ != nullptr) {
        last_status_power_ = power_text;
        lv_label_set_text(status_power_label_, power_text.c_str());
    }
}

}  // namespace el_sistema
