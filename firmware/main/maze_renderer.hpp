#pragma once
#include "maze_model.hpp"
#include "lvgl.h"
namespace el_sistema {
class MazeRenderer {
public:
    void init(lv_obj_t* parent,const MazeLevel& level);
    void draw(const MazeSnapshot& snapshot);
private:
    lv_obj_t* ball_=nullptr;
    int last_x_=-1,last_y_=-1;
    bool fallen_=false;
};
}
