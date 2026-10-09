#include "ui.hpp"
#include "fonts/fonts.h"
#include "esp_timer.h"
#include <cstdio>
namespace el_sistema {
namespace {
std::string maze_time(uint32_t ms) {
    char text[40];const unsigned seconds=ms/1000;
    if(seconds>=3600) std::snprintf(text,sizeof(text),"%02u:%02u:%02u",seconds/3600,(seconds/60)%60,seconds%60);
    else std::snprintf(text,sizeof(text),"%02u:%02u",seconds/60,seconds%60);
    return text;
}
}
void AppUi::render_maze() {
    // Mutex contention is transient, not an unavailable service. Retain the
    // last good snapshot so a busy read cannot remove the Start/Resume controls.
    const auto snapshot=maze_.snapshot();
    if(snapshot.available||snapshot.error==MazeError::Unavailable) maze_snapshot_=snapshot;
    const auto& game=maze_snapshot_;
    auto* title=label(screen_,"LABERINTO",&font_rajdhani_40,0xE4AA60);lv_obj_set_pos(title,24,20);
    button(screen_,"INICIO",544,20,152,52,ActionCode::GoHome);
    maze_status_=label(screen_,"",&font_rajdhani_28,0xF0F7FF);lv_obj_set_pos(maze_status_,24,85);
    auto* hint=label(screen_,game.state==MazeState::Calibrating?"CALIBRANDO · MANTÉN LA TAB5 QUIETA":"INCLINA PARA MOVER · ALCANZA LA META VERDE",
                     &font_rajdhani_20,0xA7B4C4);lv_obj_set_pos(hint,24,132);
    auto* board=lv_obj_create(screen_);lv_obj_set_pos(board,60,170);lv_obj_set_size(board,MAZE_WIDTH,MAZE_HEIGHT);
    lv_obj_set_style_pad_all(board,0,0);lv_obj_set_style_border_width(board,0,0);lv_obj_set_style_radius(board,0,0);
    lv_obj_set_style_bg_color(board,lv_color_hex(0x241C14),0);lv_obj_set_style_bg_opa(board,LV_OPA_COVER,0);
    lv_obj_remove_flag(board,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE));
    maze_renderer_=std::make_unique<MazeRenderer>();maze_renderer_->init(board,make_maze_level(game.level));
    maze_renderer_->draw(game);
    if(game.state==MazeState::Running||game.state==MazeState::Calibrating) {
        button(screen_,"PAUSA",60,1120,600,64,ActionCode::MazePause);
    } else {
        auto* menu=panel(screen_,48,260,624,760);lv_obj_set_style_bg_opa(menu,LV_OPA_90,0);
        const char* heading=game.state==MazeState::Paused?"PAUSA":game.state==MazeState::Fallen?"BOLA PERDIDA":
                            game.state==MazeState::Won?"¡META ALCANZADA!":"ELIGE NIVEL";
        auto* caption=label(menu,heading,&font_rajdhani_40,game.state==MazeState::Won?0x61E2AC:0xE4AA60);
        lv_obj_set_pos(caption,24,22);lv_obj_set_width(caption,576);
        const char* instructions=game.available?(game.error!=MazeError::None?maze_error_text(game.error):
            game.state==MazeState::Fallen?"Evita los agujeros oscuros. Puedes reintentar el nivel.":
            game.state==MazeState::Won?"¡Buen equilibrio! Puedes elegir otro nivel o mejorar tu tiempo.":
            "Coloca la Tab5 casi horizontal y quieta para calibrar. Luego inclínala suavemente para guiar la bola."):
            "No se pudo iniciar el juego. El resto de los módulos sigue disponible.";
        auto* help=label(menu,instructions,&font_rajdhani_28,0xB6C4DA);lv_obj_set_pos(help,24,85);lv_obj_set_width(help,576);
        const uint32_t best=game.best_ms[game.level];
        const std::string record=best?"MEJOR TIEMPO: "+maze_time(best):"MEJOR TIEMPO: —";
        auto* best_label=label(menu,record.c_str(),&font_rajdhani_28,0xFFE14F);lv_obj_set_pos(best_label,24,235);
        if(game.available) {
            int level_y=410;
            if(game.state==MazeState::Paused) {
                button(menu,"REANUDAR",24,300,576,60,ActionCode::MazeResume);
                button(menu,"CALIBRAR Y REANUDAR",24,380,576,60,ActionCode::MazeCalibrate);
                button(menu,"REINTENTAR DESDE EL INICIO",24,460,576,60,ActionCode::MazeStart);
                level_y=560;
            } else if(game.state==MazeState::Won) {
                if(game.level+1<MAZE_LEVELS)
                    button(menu,"SIGUIENTE NIVEL",24,300,576,60,ActionCode::MazeSelect,0,game.level+1);
                button(menu,"REINTENTAR",24,380,576,60,ActionCode::MazeStart);level_y=480;
            } else button(menu,game.state==MazeState::Ready?"INICIAR Y CALIBRAR":"REINTENTAR",24,300,576,60,ActionCode::MazeStart);
            for(int level=0;level<MAZE_LEVELS;++level) {
                char text[32];std::snprintf(text,sizeof(text),"NIVEL %d",level+1);
                button(menu,text,24+level*192,level_y,176,60,ActionCode::MazeSelect,0,level,nullptr,
                       unsigned(level)==game.level?0x553D27:0x182335,0xA87843);
            }
        }
    }
    if(game.storage_error) {
        auto* warning=label(screen_,"No se pudo guardar el récord; sigue disponible en RAM.",&font_rajdhani_20,0xFF5A69);
        lv_obj_set_pos(warning,24,1220);lv_obj_set_width(warning,672);
    }
    maze_timer_=lv_timer_create(maze_animation_callback,30,this);
    update_maze();
}
void AppUi::maze_animation_callback(lv_timer_t* timer) {
    static_cast<AppUi*>(lv_timer_get_user_data(timer))->update_maze();
}
void AppUi::update_maze() {
    if(view_!=View::Maze||screen_off_||render_pending_) return;
    const auto game=maze_.snapshot();if(!game.available) return;
    if(game.state!=maze_snapshot_.state||game.level!=maze_snapshot_.level||game.error!=maze_snapshot_.error||
       game.storage_error!=maze_snapshot_.storage_error) {maze_snapshot_=game;schedule_render();return;}
    maze_snapshot_=game;
    if(maze_renderer_) maze_renderer_->draw(game);
    if(maze_status_) {
        char text[120];const auto time=maze_time(game.elapsed_ms);
        std::snprintf(text,sizeof(text),"NIVEL %u · %s · INTENTOS %u",game.level+1,time.c_str(),game.attempts);
        if(std::string(lv_label_get_text(maze_status_))!=text) lv_label_set_text(maze_status_,text);
    }
}
void AppUi::leave_maze() {
    maze_.set_visible(false);
    const int detected=orientation_.poll();current_rotation_=detected>=0?detected:maze_return_rotation_;
    orientation_.set_current(current_rotation_);
    static const lv_display_rotation_t rotations[]={LV_DISPLAY_ROTATION_0,LV_DISPLAY_ROTATION_90,
                                                   LV_DISPLAY_ROTATION_180,LV_DISPLAY_ROTATION_270};
    lv_display_set_rotation(display_,rotations[current_rotation_]);
    home_rotation_pending_=false;home_rotation_sources_.clear();
}
}
