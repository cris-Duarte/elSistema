#pragma once
#include "tetris_model.hpp"
#include "lvgl.h"
namespace el_sistema {
const lv_font_t* tetris_font();
uint32_t tetris_color(unsigned kind);
class TetrisRenderer {
public:
    void init(lv_obj_t* board,int cell_size,int columns);
    void draw(const TetrisSnapshot& state);
private:
    std::array<lv_obj_t*,TETRIS_VISIBLE_CELLS> cells_{};
    std::array<uint8_t,TETRIS_VISIBLE_CELLS> previous_{};
    int columns_=TETRIS_COLUMNS;
};
}
