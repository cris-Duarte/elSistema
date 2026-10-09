#include "maze_renderer.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace el_sistema;
static unsigned flushes=0;
static void flush(lv_display_t* display,const lv_area_t*,uint8_t*) {++flushes;lv_display_flush_ready(display);}
int main() {
    lv_init();auto* display=lv_display_create(720,1280);std::vector<uint8_t> buffer(720*128*4);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,buffer.data(),nullptr,buffer.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    for(unsigned index=0;index<MAZE_LEVELS;++index) {
        auto* board=lv_obj_create(lv_screen_active());lv_obj_set_pos(board,60,170);lv_obj_set_size(board,MAZE_WIDTH,MAZE_HEIGHT);
        lv_obj_set_style_pad_all(board,0,0);lv_obj_set_style_border_width(board,0,0);lv_obj_set_style_radius(board,0,0);
        lv_obj_remove_flag(board,LV_OBJ_FLAG_SCROLLABLE);
        const auto level=make_maze_level(index);MazeRenderer renderer;renderer.init(board,level);
        MazeSnapshot state;state.level=index;
        const auto objects=lv_obj_get_child_count(board);assert(objects==level.wall_count+level.hole_count+2);
        for(int frame=0;frame<500;++frame) {
            advance_maze_ball(state.ball,level,std::sin(frame*.08f),std::cos(frame*.11f),.03f);
            renderer.draw(state);lv_tick_inc(30);lv_timer_handler();assert(lv_obj_get_child_count(board)==objects);
            lv_area_t bounds{};lv_obj_get_coords(lv_obj_get_child(board,objects-1),&bounds);
            assert(bounds.x1>=60&&bounds.x2<660&&bounds.y1>=170&&bounds.y2<1070);
        }
        state.state=MazeState::Fallen;renderer.draw(state);state.state=MazeState::Running;renderer.draw(state);
        lv_obj_delete(board);
    }
    lv_deinit();assert(flushes>0);std::printf("LVGL: tres tableros, bola dentro del área, objetos estables y %u flushes OK\n",flushes);
}
