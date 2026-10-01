#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bsp/m5stack_tab5.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fonts/fonts.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

namespace {

constexpr uint32_t COLOR_BG = 0x050711;
constexpr uint32_t COLOR_PANEL = 0x0C1122;
constexpr uint32_t COLOR_PANEL_ALT = 0x0F1730;
constexpr uint32_t COLOR_LINE = 0x26375A;
constexpr uint32_t COLOR_CYAN = 0x6CE5FF;
constexpr uint32_t COLOR_CYAN_SOFT = 0xB7F3FF;
constexpr uint32_t COLOR_VIOLET = 0x9B7CFF;
constexpr uint32_t COLOR_TEXT = 0xF4F7FF;
constexpr uint32_t COLOR_MUTED = 0x8792AD;
constexpr uint32_t COLOR_SUCCESS = 0x7FFFC6;
constexpr uint32_t COLOR_WARNING = 0xFFCF70;

constexpr int XP_PER_LEVEL = 500;

enum class MissionState {
    Backlog,
    FocusPaused,
    FocusActive,
    Completed,
};

struct Mission {
    const char* title;
    const char* description;
    char rank;
    MissionState state;
    uint32_t elapsed_seconds;
    int64_t active_since_us;
};

Mission missions[] = {
    {"Construir la base de El Sistema", "Interfaz nativa LVGL y flujo inicial de enfoque.", 'B', MissionState::FocusActive, 27 * 60 + 34, 0},
    {"Configurar almacenamiento NVS", "Persistir misiones, intervalos y progreso.", 'C', MissionState::FocusPaused, 18 * 60 + 12, 0},
    {"Validar micrófonos duales", "Captura de audio para el futuro servicio de dictado.", 'C', MissionState::FocusPaused, 6 * 60 + 48, 0},
    {"Diseñar teclado táctil", "Registro escrito de una nueva misión.", 'D', MissionState::Backlog, 0, 0},
    {"Probar recordatorios hablados", "Aviso local para retomar una misión pausada.", 'A', MissionState::Backlog, 0, 0},
};

constexpr size_t MISSION_COUNT = sizeof(missions) / sizeof(missions[0]);

int active_mission = 0;
int level = 7;
int xp = 340;
bool reward_visible = false;
bool info_visible = false;
int last_reward = 0;
const char* info_title = "Próxima iteración";
const char* info_body = "Esta función quedará conectada al servicio correspondiente.";

lv_obj_t* screen = nullptr;
lv_obj_t* timer_label = nullptr;
lv_obj_t* live_label = nullptr;
lv_obj_t* pause_label = nullptr;

lv_color_t color(uint32_t value)
{
    return lv_color_hex(value);
}

uint32_t current_elapsed(const Mission& mission)
{
    uint32_t elapsed = mission.elapsed_seconds;
    if (mission.state == MissionState::FocusActive && mission.active_since_us > 0) {
        elapsed += static_cast<uint32_t>((esp_timer_get_time() - mission.active_since_us) / 1000000);
    }
    return elapsed;
}

void commit_elapsed(Mission& mission)
{
    if (mission.state == MissionState::FocusActive && mission.active_since_us > 0) {
        mission.elapsed_seconds = current_elapsed(mission);
        mission.active_since_us = 0;
    }
}

void format_duration(uint32_t total_seconds, char* output, size_t output_size)
{
    const uint32_t hours = total_seconds / 3600;
    const uint32_t minutes = (total_seconds % 3600) / 60;
    const uint32_t seconds = total_seconds % 60;
    std::snprintf(output, output_size, "%02lu:%02lu:%02lu", static_cast<unsigned long>(hours),
                  static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
}

uint32_t rank_color(char rank)
{
    switch (rank) {
        case 'E': return 0x8FA1BD;
        case 'D': return 0x76D7B1;
        case 'C': return COLOR_CYAN;
        case 'B': return 0x778CFF;
        case 'A': return 0xB57CFF;
        case 'S': return COLOR_WARNING;
        default: return COLOR_CYAN;
    }
}

int rank_base_xp(char rank)
{
    switch (rank) {
        case 'E': return 25;
        case 'D': return 50;
        case 'C': return 100;
        case 'B': return 200;
        case 'A': return 350;
        case 'S': return 600;
        default: return 0;
    }
}

void style_container(lv_obj_t* object, uint32_t background, uint32_t border = COLOR_LINE)
{
    lv_obj_set_style_bg_color(object, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(object, color(border), LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(object, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* make_label(lv_obj_t* parent, const char* text, const lv_font_t* font, uint32_t text_color)
{
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color(text_color), LV_PART_MAIN);
    return label;
}

lv_obj_t* make_button(lv_obj_t* parent, const char* text, int x, int y, int width, int height, uint32_t background,
                      uint32_t border, lv_event_cb_t callback, void* user_data = nullptr)
{
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, color(border), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 3, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button,
        color(COLOR_VIOLET),
        static_cast<lv_style_selector_t>(LV_PART_MAIN) |
            static_cast<lv_style_selector_t>(LV_STATE_PRESSED));
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    lv_obj_t* label = make_label(button, text, &font_es_16, COLOR_TEXT);
    lv_obj_center(label);
    return button;
}

lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int width, int height)
{
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    style_container(panel, COLOR_PANEL);

    lv_obj_t* accent = lv_obj_create(panel);
    lv_obj_set_pos(accent, 18, 0);
    lv_obj_set_size(accent, 120, 2);
    lv_obj_set_style_bg_color(accent, color(COLOR_CYAN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(accent, 0, LV_PART_MAIN);
    return panel;
}

void schedule_render();

void pause_event(lv_event_t*)
{
    if (active_mission < 0) return;
    Mission& mission = missions[active_mission];
    if (mission.state == MissionState::FocusActive) {
        commit_elapsed(mission);
        mission.state = MissionState::FocusPaused;
    } else {
        mission.state = MissionState::FocusActive;
        mission.active_since_us = esp_timer_get_time();
    }
    schedule_render();
}

void switch_event(lv_event_t* event)
{
    const int next_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    if (next_index < 0 || next_index >= static_cast<int>(MISSION_COUNT) || next_index == active_mission) return;

    if (active_mission >= 0) {
        Mission& current = missions[active_mission];
        commit_elapsed(current);
        if (current.state != MissionState::Completed) current.state = MissionState::FocusPaused;
    }

    Mission& next = missions[next_index];
    next.state = MissionState::FocusActive;
    next.active_since_us = esp_timer_get_time();
    active_mission = next_index;
    schedule_render();
}

void complete_event(lv_event_t*)
{
    if (active_mission < 0) return;

    Mission& mission = missions[active_mission];
    commit_elapsed(mission);
    const int active_minutes = std::min<int>(mission.elapsed_seconds / 60, 120);
    last_reward = rank_base_xp(mission.rank) + active_minutes * 2;
    xp += last_reward;
    while (xp >= XP_PER_LEVEL) {
        xp -= XP_PER_LEVEL;
        ++level;
    }
    mission.state = MissionState::Completed;

    active_mission = -1;
    for (size_t i = 0; i < MISSION_COUNT; ++i) {
        if (missions[i].state == MissionState::FocusPaused) {
            active_mission = static_cast<int>(i);
            missions[i].state = MissionState::FocusActive;
            missions[i].active_since_us = esp_timer_get_time();
            break;
        }
    }

    reward_visible = true;
    schedule_render();
}

void close_modal_event(lv_event_t*)
{
    reward_visible = false;
    info_visible = false;
    schedule_render();
}

void new_mission_event(lv_event_t*)
{
    info_title = "Nueva misión";
    info_body = "El teclado táctil y el formulario de registro llegan en la siguiente iteración.";
    info_visible = true;
    schedule_render();
}

void dictation_event(lv_event_t*)
{
    info_title = "Dictado";
    info_body = "Los micrófonos se integrarán como un servicio independiente después de validar la interfaz base.";
    info_visible = true;
    schedule_render();
}

void history_event(lv_event_t*)
{
    int completed = 0;
    for (const auto& mission : missions) {
        if (mission.state == MissionState::Completed) ++completed;
    }
    static char message[96];
    std::snprintf(message, sizeof(message), "%d misión%s completada%s en esta sesión de prueba.", completed,
                  completed == 1 ? "" : "es", completed == 1 ? "" : "s");
    info_title = "Misiones completadas";
    info_body = message;
    info_visible = true;
    schedule_render();
}

void create_header()
{
    lv_obj_t* brand_mark = lv_obj_create(screen);
    lv_obj_set_pos(brand_mark, 26, 22);
    lv_obj_set_size(brand_mark, 48, 48);
    style_container(brand_mark, COLOR_PANEL_ALT, COLOR_CYAN);
    lv_obj_t* symbol = make_label(brand_mark, "S", &lv_font_montserrat_24, COLOR_CYAN);
    lv_obj_center(symbol);

    lv_obj_t* brand = make_label(screen, "EL SISTEMA", &font_es_20, COLOR_TEXT);
    lv_obj_set_pos(brand, 90, 31);
    lv_obj_set_style_text_letter_space(brand, 3, LV_PART_MAIN);

    char level_text[24];
    std::snprintf(level_text, sizeof(level_text), "NV. %02d", level);
    lv_obj_t* level_label = make_label(screen, level_text, &lv_font_montserrat_24, COLOR_TEXT);
    lv_obj_set_pos(level_label, 440, 20);

    lv_obj_t* xp_copy = make_label(screen, "EXPERIENCIA", &font_es_14, COLOR_MUTED);
    lv_obj_set_pos(xp_copy, 550, 15);
    char xp_text[40];
    std::snprintf(xp_text, sizeof(xp_text), "%d / %d XP", xp, XP_PER_LEVEL);
    lv_obj_t* xp_value = make_label(screen, xp_text, &font_es_14, COLOR_CYAN_SOFT);
    lv_obj_set_pos(xp_value, 760, 15);

    lv_obj_t* xp_bar = lv_bar_create(screen);
    lv_obj_set_pos(xp_bar, 550, 45);
    lv_obj_set_size(xp_bar, 300, 8);
    lv_bar_set_range(xp_bar, 0, XP_PER_LEVEL);
    lv_bar_set_value(xp_bar, xp, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(xp_bar, color(0x17213A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(xp_bar, color(COLOR_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(xp_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(xp_bar, 0, LV_PART_INDICATOR);

    make_button(screen, "Completadas", 914, 20, 160, 52, COLOR_PANEL, COLOR_LINE, history_event);
    make_button(screen, "+  Nueva misión", 1088, 20, 168, 52, 0x394BAF, COLOR_VIOLET, new_mission_event);

    lv_obj_t* divider = lv_obj_create(screen);
    lv_obj_set_pos(divider, 24, 91);
    lv_obj_set_size(divider, 1232, 1);
    lv_obj_set_style_bg_color(divider, color(COLOR_LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
}

void create_active_panel()
{
    lv_obj_t* panel = make_panel(screen, 24, 110, 752, 586);

    lv_obj_t* eyebrow = make_label(panel, "ZONA DE ENFOQUE", &font_es_14, COLOR_CYAN);
    lv_obj_set_pos(eyebrow, 24, 24);
    lv_obj_t* heading = make_label(panel, "Misión activa", &font_es_20, COLOR_TEXT);
    lv_obj_set_pos(heading, 24, 48);

    if (active_mission < 0) {
        lv_obj_t* empty = make_label(panel, "La zona está libre", &font_es_28, COLOR_TEXT);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, -20);
        lv_obj_t* hint = make_label(panel, "Activa una misión del registro para comenzar.", &font_es_16, COLOR_MUTED);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 26);
        return;
    }

    Mission& mission = missions[active_mission];
    const bool running = mission.state == MissionState::FocusActive;
    live_label = make_label(panel, running ? "EN CURSO" : "EN PAUSA", &font_es_14,
                            running ? COLOR_SUCCESS : COLOR_MUTED);
    lv_obj_set_pos(live_label, 620, 28);

    lv_obj_t* rank_box = lv_obj_create(panel);
    lv_obj_set_pos(rank_box, 338, 96);
    lv_obj_set_size(rank_box, 76, 76);
    style_container(rank_box, COLOR_PANEL_ALT, rank_color(mission.rank));
    char rank_text[2] = {mission.rank, '\0'};
    lv_obj_t* rank_label = make_label(rank_box, rank_text, &lv_font_montserrat_32, rank_color(mission.rank));
    lv_obj_center(rank_label);

    lv_obj_t* title = make_label(panel, mission.title, &font_es_28, COLOR_TEXT);
    lv_obj_set_width(title, 650);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 190);

    lv_obj_t* description = make_label(panel, mission.description, &font_es_16, COLOR_MUTED);
    lv_obj_set_width(description, 650);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(description, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 232);

    char duration[24];
    format_duration(current_elapsed(mission), duration, sizeof(duration));
    timer_label = make_label(panel, duration, &lv_font_montserrat_48, COLOR_TEXT);
    lv_obj_align(timer_label, LV_ALIGN_TOP_MID, 0, 300);

    lv_obj_t* timer_caption = make_label(panel, "TIEMPO ACTIVO", &font_es_14, COLOR_MUTED);
    lv_obj_align(timer_caption, LV_ALIGN_TOP_MID, 0, 362);

    lv_obj_t* pause_button = make_button(panel, running ? "II  Pausar" : ">  Reanudar", 115, 446, 220, 64,
                                         COLOR_PANEL_ALT, COLOR_LINE, pause_event);
    pause_label = lv_obj_get_child(pause_button, 0);
    make_button(panel, "Completar misión", 351, 446, 286, 64, 0x183A52, COLOR_CYAN, complete_event);

    lv_obj_t* session = make_label(panel, "Sesión local · cronómetro monotónico", &font_es_14, COLOR_MUTED);
    lv_obj_set_pos(session, 24, 548);
}

void create_mission_card(lv_obj_t* parent, int mission_index, int y, bool backlog)
{
    const Mission& mission = missions[mission_index];
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_pos(card, 18, y);
    lv_obj_set_size(card, 416, 72);
    style_container(card, 0x0E162B, COLOR_LINE);

    lv_obj_t* rank = lv_obj_create(card);
    lv_obj_set_pos(rank, 12, 14);
    lv_obj_set_size(rank, 44, 44);
    style_container(rank, COLOR_PANEL_ALT, rank_color(mission.rank));
    char rank_text[2] = {mission.rank, '\0'};
    lv_obj_t* rank_label = make_label(rank, rank_text, &lv_font_montserrat_18, rank_color(mission.rank));
    lv_obj_center(rank_label);

    lv_obj_t* title = make_label(card, mission.title, &font_es_16, COLOR_TEXT);
    lv_obj_set_pos(title, 70, 13);
    lv_obj_set_width(title, 235);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

    char timing[40];
    if (mission.elapsed_seconds == 0) {
        std::snprintf(timing, sizeof(timing), "Sin iniciar");
    } else {
        std::snprintf(timing, sizeof(timing), "%lu min acumulados",
                      static_cast<unsigned long>(mission.elapsed_seconds / 60));
    }
    lv_obj_t* meta = make_label(card, timing, &font_es_14, COLOR_MUTED);
    lv_obj_set_pos(meta, 70, 40);

    make_button(card, backlog ? "Activar" : "Cambiar", 316, 15, 88, 42, COLOR_PANEL_ALT, COLOR_CYAN,
                switch_event, reinterpret_cast<void*>(static_cast<intptr_t>(mission_index)));
}

void create_queue_panel()
{
    lv_obj_t* panel = make_panel(screen, 794, 110, 462, 278);
    lv_obj_t* eyebrow = make_label(panel, "EN ESPERA", &font_es_14, COLOR_MUTED);
    lv_obj_set_pos(eyebrow, 20, 20);
    lv_obj_t* heading = make_label(panel, "Cola de enfoque", &font_es_20, COLOR_TEXT);
    lv_obj_set_pos(heading, 20, 44);

    int count = 0;
    int y = 88;
    for (size_t i = 0; i < MISSION_COUNT && count < 2; ++i) {
        if (missions[i].state == MissionState::FocusPaused) {
            create_mission_card(panel, static_cast<int>(i), y, false);
            y += 80;
            ++count;
        }
    }
    if (count == 0) {
        lv_obj_t* empty = make_label(panel, "No hay misiones esperando.", &font_es_16, COLOR_MUTED);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 28);
    }
}

void create_backlog_panel()
{
    lv_obj_t* panel = make_panel(screen, 794, 406, 462, 290);
    lv_obj_t* eyebrow = make_label(panel, "REGISTRO", &font_es_14, COLOR_MUTED);
    lv_obj_set_pos(eyebrow, 20, 20);
    lv_obj_t* heading = make_label(panel, "Misiones disponibles", &font_es_20, COLOR_TEXT);
    lv_obj_set_pos(heading, 20, 44);

    int count = 0;
    int y = 88;
    for (size_t i = 0; i < MISSION_COUNT && count < 2; ++i) {
        if (missions[i].state == MissionState::Backlog) {
            create_mission_card(panel, static_cast<int>(i), y, true);
            y += 80;
            ++count;
        }
    }
    if (count == 0) {
        lv_obj_t* empty = make_label(panel, "Registro despejado.", &font_es_16, COLOR_MUTED);
        lv_obj_align(empty, LV_ALIGN_CENTER, 0, 28);
    }
}

void create_floating_dictation_button()
{
    lv_obj_t* button = make_button(screen, "MIC  Dictar", 1094, 642, 146, 44, 0x263B73, COLOR_CYAN, dictation_event);
    lv_obj_set_style_radius(button, 22, LV_PART_MAIN);
}

void create_modal(const char* title, const char* body, const char* action)
{
    lv_obj_t* overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, 1280, 720);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_color(overlay, color(0x02040A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, 210, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* card = lv_obj_create(overlay);
    lv_obj_set_size(card, 560, 300);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    style_container(card, COLOR_PANEL_ALT, COLOR_CYAN);

    lv_obj_t* eyebrow = make_label(card, reward_visible ? "MISIÓN COMPLETADA" : "EL SISTEMA",
                                   &font_es_14, COLOR_CYAN);
    lv_obj_align(eyebrow, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_t* heading = make_label(card, title, &font_es_28, COLOR_TEXT);
    lv_obj_set_width(heading, 500);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_t* copy = make_label(card, body, &font_es_16, COLOR_MUTED);
    lv_obj_set_width(copy, 470);
    lv_label_set_long_mode(copy, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(copy, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(copy, LV_ALIGN_TOP_MID, 0, 126);
    make_button(card, action, 170, 222, 220, 54, 0x394BAF, COLOR_VIOLET, close_modal_event);
}

void render_dashboard()
{
    timer_label = nullptr;
    live_label = nullptr;
    pause_label = nullptr;

    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, color(COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    create_header();
    create_active_panel();
    create_queue_panel();
    create_backlog_panel();
    create_floating_dictation_button();

    if (reward_visible) {
        char reward_copy[128];
        std::snprintf(reward_copy, sizeof(reward_copy), "+%d XP obtenidos. La siguiente misión ya está lista.", last_reward);
        create_modal("Objetivo conquistado", reward_copy, "Continuar");
    } else if (info_visible) {
        create_modal(info_title, info_body, "Entendido");
    }
}

void render_async(void*)
{
    render_dashboard();
}

void schedule_render()
{
    lv_async_call(render_async, nullptr);
}

void timer_callback(lv_timer_t*)
{
    if (active_mission < 0 || timer_label == nullptr) return;
    char duration[24];
    format_duration(current_elapsed(missions[active_mission]), duration, sizeof(duration));
    lv_label_set_text(timer_label, duration);
}

void init_display()
{
    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_io_expander_pi4ioe_init(bsp_i2c_get_handle());
    bsp_reset_tp();

    bsp_display_cfg_t config = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .double_buffer = true,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        },
    };

    lv_display_t* display = bsp_display_start_with_config(&config);
    if (display == nullptr) {
        ESP_LOGE("el_sistema", "No se pudo iniciar el display");
        abort();
    }

    lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    ESP_ERROR_CHECK(bsp_display_backlight_on());
    ESP_ERROR_CHECK(bsp_display_brightness_set(72));

    missions[active_mission].active_since_us = esp_timer_get_time();

    bsp_display_lock(0);
    screen = lv_screen_active();
    render_dashboard();
    lv_timer_create(timer_callback, 250, nullptr);
    bsp_display_unlock();
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI("el_sistema", "Iniciando El Sistema");
    init_display();
    ESP_LOGI("el_sistema", "Interfaz lista");
}
