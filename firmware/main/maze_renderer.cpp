#include "maze_renderer.hpp"
#include <cmath>
namespace el_sistema {
namespace {
lv_obj_t* shape(lv_obj_t* parent,int x,int y,int w,int h,uint32_t color,int radius=0) {
    auto* o=lv_obj_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,radius,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_remove_flag(o,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));return o;
}
}
void MazeRenderer::init(lv_obj_t* parent,const MazeLevel& level) {
    for(unsigned w=0;w<level.wall_count;++w) {
        const auto& r=level.walls[w];auto* wall=shape(parent,int(r.x),int(r.y),int(r.w),int(r.h),0xA87843);
        lv_obj_set_style_border_width(wall,1,0);lv_obj_set_style_border_color(wall,lv_color_hex(0xD0A169),0);
    }
    for(unsigned h=0;h<level.hole_count;++h) {
        auto* hole=shape(parent,int(level.holes[h].x)-18,int(level.holes[h].y)-18,36,36,0x070707,LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(hole,2,0);lv_obj_set_style_border_color(hole,lv_color_hex(0x60452B),0);
    }
    auto* goal=shape(parent,int(level.goal.x)-26,int(level.goal.y)-26,52,52,0x1B725A,LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(goal,3,0);lv_obj_set_style_border_color(goal,lv_color_hex(0x61E2AC),0);
    shape(goal,19,19,8,8,0xACFFD7,LV_RADIUS_CIRCLE);
    ball_=shape(parent,36,36,28,28,0xB9C8D5,LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ball_,2,0);lv_obj_set_style_border_color(ball_,lv_color_hex(0x6B7D90),0);
    shape(ball_,5,4,8,8,0xF5FAFF,LV_RADIUS_CIRCLE);
}
void MazeRenderer::draw(const MazeSnapshot& s) {
    const int x=int(std::lround(s.ball.x))-14,y=int(std::lround(s.ball.y))-14;
    if(x!=last_x_||y!=last_y_) {lv_obj_set_pos(ball_,x,y);last_x_=x;last_y_=y;}
    const bool fallen=s.state==MazeState::Fallen;
    if(fallen!=fallen_) {lv_obj_set_style_opa(ball_,fallen?LV_OPA_30:LV_OPA_COVER,0);fallen_=fallen;}
}
}
