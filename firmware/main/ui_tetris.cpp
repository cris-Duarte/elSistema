#include "ui.hpp"
#include "tetris_layout.hpp"
#include "fonts/fonts.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cstdio>
namespace el_sistema {
namespace {
void rect(lv_obj_t* o,int x,int y,int w,int h) {
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_radius(o,0,0);lv_obj_set_style_border_width(o,0,0);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
}
}
void AppUi::render_tetris() {
    const auto s=tetris_.snapshot();if(s.available) tetris_snapshot_=s;
    tetris_last_state_=tetris_snapshot_.state;tetris_last_level_=tetris_snapshot_.level;
    const auto& game=tetris_snapshot_;
    auto* title=label(screen_,"TETRIS",tetris_font(),0xB979FF);
    lv_obj_set_pos(title,24,24);
    if(!tetris_playing_) {
        lv_obj_set_style_transform_scale(title,256*2,0);lv_obj_set_pos(title,100,110);
        char record[64];std::snprintf(record,sizeof(record),"RECORD %lu",(unsigned long)game.record);
        auto* best=label(screen_,record,tetris_font(),0xFFE14F);lv_obj_set_pos(best,80,220);
        auto* help=label(screen_,"DESLIZA A LOS LADOS PARA MOVER\nTOCA PARA GIRAR\nARRASTRA ABAJO Y MANTÉN PARA ACELERAR",
                         &font_rajdhani_28,0xB6C4DA);
        lv_obj_set_pos(help,60,330);lv_obj_set_width(help,600);
        if(game.columns==10&&game.state==TetrisState::Paused) {
            auto* classic=label(screen_,"CONTINUAR: TABLERO ORIGINAL DE 10 COLUMNAS\nNUEVA PARTIDA: 12 COLUMNAS",
                                &font_rajdhani_20,0xB6C4DA);
            lv_obj_set_pos(classic,60,470);lv_obj_set_width(classic,600);
        }
        if(game.available) {
            button(screen_,"NUEVA PARTIDA",80,570,560,72,ActionCode::TetrisNew,0,0,nullptr,0x272143,0xB979FF);
            if(game.state==TetrisState::Paused)
                button(screen_,"CONTINUAR",80,660,560,72,ActionCode::TetrisContinue);
            button(screen_,"SONIDO",80,750,560,64,ActionCode::TetrisSound);
        }
        button(screen_,"INICIO",80,1080,560,64,ActionCode::GoHome);
    } else {
        using namespace tetris_layout;
        lv_obj_set_pos(title,12,4);
        tetris_score_=label(screen_,"",&font_rajdhani_20,0xF0F7FF);lv_obj_set_pos(tetris_score_,176,8);
        lv_obj_set_width(tetris_score_,210);lv_label_set_long_mode(tetris_score_,LV_LABEL_LONG_CLIP);
        tetris_record_=label(screen_,"",&font_rajdhani_20,0xFFE14F);lv_obj_set_pos(tetris_record_,176,42);
        lv_obj_set_width(tetris_record_,210);lv_label_set_long_mode(tetris_record_,LV_LABEL_LONG_CLIP);
        tetris_level_=label(screen_,"",&font_rajdhani_20,0x46E874);lv_obj_set_pos(tetris_level_,12,36);
        lv_obj_set_size(tetris_level_,150,42);lv_obj_set_style_text_line_space(tetris_level_,0,0);
        button(screen_,"PAUSA",pause_x,pause_y,pause_width,pause_height,ActionCode::TetrisPause);
        auto* board=lv_obj_create(screen_);rect(board,board_x(game.columns),header_height,cell_width*game.columns,board_height);
        lv_obj_set_style_bg_color(board,lv_color_hex(0x0A101C),0);
        lv_obj_add_event_cb(board,tetris_touch_callback,LV_EVENT_ALL,this);
        tetris_renderer_=std::make_unique<TetrisRenderer>();tetris_renderer_->init(board,cell_width,game.columns);
        auto* next=label(screen_,"SIGUE",&font_rajdhani_20,0xB6C4DA);lv_obj_set_pos(next,preview_x,8);
        for(auto& piece:tetris_preview_) {
            piece=lv_obj_create(screen_);rect(piece,preview_x,preview_y,preview_cell,preview_cell);
            lv_obj_set_style_bg_opa(piece,LV_OPA_COVER,0);lv_obj_set_style_border_width(piece,2,0);
            lv_obj_set_style_border_color(piece,lv_color_hex(0x536078),0);
            lv_obj_remove_flag(piece,LV_OBJ_FLAG_CLICKABLE);
        }
        tetris_notice_=label(screen_,"",&font_rajdhani_20,0xFFE14F);
        lv_obj_set_pos(tetris_notice_,484,14);lv_obj_set_size(tetris_notice_,116,52);
        tetris_renderer_->draw(game);
        // A paused/finished game uses a stable menu; the board stays visible
        // behind it without reconstructing any objects on normal game frames.
        if(game.state!=TetrisState::Running) {
            auto* menu=panel(screen_,72,360,576,580);
            lv_obj_set_style_bg_opa(menu,LV_OPA_90,0);
            auto* heading=label(menu,game.state==TetrisState::Finished?"FIN DE PARTIDA":"PAUSA",tetris_font(),0xFFE14F);
            lv_obj_set_pos(heading,32,32);
            if(game.state==TetrisState::Paused) button(menu,"REANUDAR",32,118,512,68,ActionCode::TetrisContinue);
            button(menu,"NUEVA PARTIDA",32,208,512,68,ActionCode::TetrisNew);
            button(menu,"SONIDO",32,298,512,68,ActionCode::TetrisSound);
            button(menu,"INICIO",32,408,512,68,ActionCode::GoHome);
        }
    }
    if(!game.available||game.storage_error||game.corrupt) {
        const char* warning=!game.available?"Juego no disponible":game.storage_error?
            "No se pudo guardar. La partida sigue en RAM.":"Se descartaron datos dañados; el récord válido se conserva.";
        auto* notice=label(screen_,warning,&font_rajdhani_28,0xFF5A69);
        lv_obj_set_pos(notice,60,960);lv_obj_set_width(notice,600);
    }
    if(tetris_sound_menu_) {
        auto* sound=panel(screen_,60,320,600,600);
        auto* caption=label(sound,"SONIDO",tetris_font(),0xB979FF);lv_obj_set_pos(caption,32,32);
        button(sound,game.music?"MÚSICA: ACTIVADA":"MÚSICA: DESACTIVADA",32,110,536,66,ActionCode::TetrisMusic);
        button(sound,game.effects?"EFECTOS: ACTIVADOS":"EFECTOS: DESACTIVADOS",32,198,536,66,ActionCode::TetrisEffects);
        char volume[64];std::snprintf(volume,sizeof(volume),"VOLUMEN %u%%",game.volume);
        auto* value=label(sound,volume,tetris_font(),0xF0F7FF);lv_obj_set_pos(value,32,294);
        button(sound,"−",32,352,250,66,ActionCode::TetrisVolume,0,-10);
        button(sound,"+",318,352,250,66,ActionCode::TetrisVolume,0,10);
        button(sound,"VOLVER",32,478,536,66,ActionCode::TetrisSound);
    }
    tetris_timer_=lv_timer_create(tetris_animation_callback,30,this);
    update_tetris();
}
void AppUi::tetris_animation_callback(lv_timer_t* timer) {
    static_cast<AppUi*>(lv_timer_get_user_data(timer))->update_tetris();
}
void AppUi::update_tetris() {
    if(view_!=View::Tetris||screen_off_||render_pending_) return;
    const auto s=tetris_.snapshot();if(!s.available) return;
    const bool settings=s.music!=tetris_snapshot_.music||s.effects!=tetris_snapshot_.effects||s.volume!=tetris_snapshot_.volume;
    const bool warning=s.corrupt!=tetris_snapshot_.corrupt||s.storage_error!=tetris_snapshot_.storage_error;
    const bool geometry=s.columns!=tetris_snapshot_.columns;
    tetris_snapshot_=s;
    if(s.clear_ms) { tetris_gesture_.cancel(); tetris_.soft_drop(false); }
    if(s.state!=tetris_last_state_||(tetris_sound_menu_&&settings)||warning||geometry) {
        tetris_gesture_.cancel();tetris_.soft_drop(false);schedule_render();return;
    }
    if(tetris_renderer_) {
        tetris_renderer_->draw(s);
        char text[96];std::snprintf(text,sizeof(text),"PUNTOS %lu",(unsigned long)s.score);
        if(tetris_score_&&std::string(lv_label_get_text(tetris_score_))!=text) lv_label_set_text(tetris_score_,text);
        std::snprintf(text,sizeof(text),"NIVEL %lu\nLÍNEAS %lu",(unsigned long)s.level,(unsigned long)s.lines);
        if(tetris_level_&&std::string(lv_label_get_text(tetris_level_))!=text) lv_label_set_text(tetris_level_,text);
        std::snprintf(text,sizeof(text),"RÉCORD %lu",(unsigned long)s.record);
        if(tetris_record_&&std::string(lv_label_get_text(tetris_record_))!=text) lv_label_set_text(tetris_record_,text);
        if(tetris_preview_kind_!=s.next) {
            tetris_preview_kind_=s.next;
            const auto preview=tetris_cells(TetrisPiece{s.next,0,0,0});
            for(size_t i=0;i<preview.size();++i) {
                if(!tetris_preview_[i]) continue;
                lv_obj_set_pos(tetris_preview_[i],tetris_layout::preview_x+preview[i].x*tetris_layout::preview_cell,
                               tetris_layout::preview_y+preview[i].y*tetris_layout::preview_cell);
                lv_obj_set_style_bg_color(tetris_preview_[i],lv_color_hex(tetris_color(s.next+1)),0);
            }
        }
        const int64_t now=esp_timer_get_time();
        if(s.level!=tetris_last_level_) {tetris_level_until_=now+1800000;tetris_last_level_=s.level;}
        if(tetris_notice_) {
            const char* notice=now<tetris_level_until_?"¡NUEVO\nNIVEL!":"";
            if(std::string(lv_label_get_text(tetris_notice_))!=notice) lv_label_set_text(tetris_notice_,notice);
        }
        ++tetris_frames_;
        if(!tetris_fps_since_) tetris_fps_since_=now;
        if(now-tetris_fps_since_>=5000000) {
            ESP_LOGI("tetris","Actualización UI %.1f Hz",tetris_frames_*1000000.f/(now-tetris_fps_since_));
            tetris_frames_=0;tetris_fps_since_=now;
        }
    }
}
void AppUi::tetris_touch_callback(lv_event_t* event) {
    auto* ui=static_cast<AppUi*>(lv_event_get_user_data(event));
    if(ui->overlay_!=Overlay::None||ui->tetris_snapshot_.state!=TetrisState::Running||ui->tetris_snapshot_.clear_ms) {
        ui->tetris_gesture_.cancel();ui->tetris_.soft_drop(false);return;
    }
    const auto code=lv_event_get_code(event);TetrisGestureAction action;
    if(code==LV_EVENT_PRESS_LOST) {ui->tetris_gesture_.cancel();ui->tetris_.soft_drop(false);return;}
    if(code==LV_EVENT_PRESSED||code==LV_EVENT_PRESSING||code==LV_EVENT_RELEASED) {
        auto* input=lv_indev_active();if(!input) return;
        lv_point_t point{};lv_indev_get_point(input,&point);
        if(code==LV_EVENT_PRESSED) ui->tetris_gesture_.press(point.x,point.y,tetris_layout::cell_width);
        else if(code==LV_EVENT_PRESSING) action=ui->tetris_gesture_.move(point.x,point.y);
        else { // Account for the final displacement before interpreting a tap.
            action=ui->tetris_gesture_.move(point.x,point.y);
            if(action.columns) ui->tetris_.move(action.columns);
            action=ui->tetris_gesture_.release();
        }
        if(action.columns) ui->tetris_.move(action.columns);
        if(action.rotate) ui->tetris_.rotate();
        if(action.soft_changed) ui->tetris_.soft_drop(action.soft);
    }
}
void AppUi::leave_tetris() {
    tetris_.set_visible(false);tetris_gesture_.cancel();
    const int detected=orientation_.poll();
    current_rotation_=detected>=0?detected:tetris_return_rotation_;
    orientation_.set_current(current_rotation_);
    static const lv_display_rotation_t rotations[]={LV_DISPLAY_ROTATION_0,LV_DISPLAY_ROTATION_90,
                                                   LV_DISPLAY_ROTATION_180,LV_DISPLAY_ROTATION_270};
    lv_display_set_rotation(display_,rotations[current_rotation_]);
    home_rotation_pending_=false;home_rotation_sources_.clear();
}
}
