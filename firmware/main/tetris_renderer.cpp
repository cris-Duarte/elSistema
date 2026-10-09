#include "tetris_renderer.hpp"
#include "fonts/fonts.h"
#include <cstring>
namespace el_sistema {
namespace {
// Original 5x7 pixel glyphs, expanded once to 24x28 A1 bitmaps.
constexpr uint8_t glyphs[][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {15,16,16,19,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
std::array<std::array<uint8_t,84>,37> bitmaps{};
int glyph_index(uint32_t letter) {
    if(letter>='0'&&letter<='9') return int(letter-'0');
    if(letter>='A'&&letter<='Z') return int(letter-'A'+10);
    if(letter==' ') return 36;
    return -1;
}
bool glyph_dsc(const lv_font_t*,lv_font_glyph_dsc_t* d,uint32_t c,uint32_t) {
    if(glyph_index(c)<0) return false;
    std::memset(d,0,sizeof(*d));d->adv_w=24;d->box_w=24;d->box_h=28;
    d->format=LV_FONT_GLYPH_FORMAT_A8;d->gid.index=c;return true;
}
const void* glyph_bitmap(lv_font_glyph_dsc_t* d,lv_draw_buf_t* buffer) {
    // LVGL 9 expects an A8 draw buffer, not a pointer to packed font bits.
    if(!buffer) return nullptr;
    const auto& bits=bitmaps[glyph_index(d->gid.index)];
    const uint32_t stride=buffer->header.stride;
    for(int y=0;y<28;++y) for(int x=0;x<24;++x)
        buffer->data[y*stride+x]=(bits[y*3+x/8]&(1u<<(7-x%8)))?255:0;
    return buffer;
}
}
const lv_font_t* tetris_font() {
    static const lv_font_t font=[] {
        for(size_t n=0;n<36;++n) for(int y=0;y<28;++y) for(int x=0;x<20;++x)
            if(glyphs[n][y/4]&(1u<<(4-x/4))) bitmaps[n][y*3+x/8]|=1u<<(7-x%8);
        lv_font_t f{};f.get_glyph_dsc=glyph_dsc;f.get_glyph_bitmap=glyph_bitmap;
        f.line_height=32;f.base_line=2;f.fallback=&font_rajdhani_28;return f;
    }();return &font;
}
uint32_t tetris_color(unsigned kind) {
    constexpr uint32_t colors[]={0x0A101C,0x35E8F3,0xFFE14F,0xB979FF,0x46E874,0xFF5A69,0x4B85FF,0xFFAC48,0xFFFFFF};
    return colors[kind<=8?kind:0];
}
void TetrisRenderer::init(lv_obj_t* board,int size,int columns) {
    columns_=columns;
    previous_.fill(255);
    for(int i=0;i<columns_*TETRIS_VISIBLE_ROWS;++i) {
        auto* c=lv_obj_create(board);cells_[i]=c;
        lv_obj_set_pos(c,(i%columns_)*size,(i/columns_)*size);lv_obj_set_size(c,size,size);
        lv_obj_set_style_radius(c,0,0);lv_obj_set_style_border_width(c,2,0);
        lv_obj_set_style_border_color(c,lv_color_hex(0x182335),0);
        lv_obj_set_style_bg_opa(c,LV_OPA_COVER,0);lv_obj_set_style_pad_all(c,0,0);
        lv_obj_remove_flag(c,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
    }
}
void TetrisRenderer::draw(const TetrisSnapshot& s) {
    if(s.columns!=columns_) return; // Geometry changes rebuild the board in UI.
    const int count=columns_*TETRIS_VISIBLE_ROWS;
    std::array<uint8_t,TETRIS_VISIBLE_CELLS> colors{};
    for(int i=0;i<count;++i) colors[i]=s.board[i+columns_*TETRIS_HIDDEN];
    if(!s.clear_ms&&(s.state==TetrisState::Running||s.state==TetrisState::Paused)) {
        for(auto c:tetris_cells(s.piece)) if(c.x>=0&&c.x<columns_&&c.y>=TETRIS_HIDDEN&&c.y<TETRIS_ROWS)
            colors[(c.y-TETRIS_HIDDEN)*columns_+c.x]=s.piece.kind+1;
    }
    if(s.clear_ms&&((s.clear_ms/50)&1)) for(int y=2;y<22;++y)
        if(s.clear_mask&(1u<<y)) for(int x=0;x<columns_;++x) colors[(y-2)*columns_+x]=8;
    for(int i=0;i<count;++i) if(colors[i]!=previous_[i]) {
        previous_[i]=colors[i];auto* c=cells_[i];
        lv_obj_set_style_bg_color(c,lv_color_hex(tetris_color(colors[i])),0);
        lv_obj_set_style_border_color(c,lv_color_hex(colors[i]?0x536078:0x182335),0);
        lv_obj_set_style_shadow_width(c,0,0);
    }
}
}
