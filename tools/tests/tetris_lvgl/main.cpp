#include "tetris_renderer.hpp"
#include "tetris_layout.hpp"
#include "fonts/fonts.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace el_sistema;
static unsigned flushed=0;
static void flush(lv_display_t* display,const lv_area_t*,uint8_t*) {++flushed;lv_display_flush_ready(display);}
int main() {
    using namespace tetris_layout;
    lv_init();auto* display=lv_display_create(screen_width,screen_height);
    std::vector<uint8_t> pixels(720*128*4);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display,pixels.data(),nullptr,pixels.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    auto* screen=lv_screen_active();
    auto* title=lv_label_create(screen);lv_obj_set_style_text_font(title,tetris_font(),0);lv_label_set_text(title,"TETRIS 0123456789");
    lv_font_glyph_dsc_t glyph{};
    assert(lv_font_get_glyph_dsc(tetris_font(),&glyph,'T',0));
    auto* bitmap=lv_draw_buf_create(glyph.box_w,glyph.box_h,LV_COLOR_FORMAT_A8,LV_STRIDE_AUTO);
    assert(bitmap&&lv_font_get_glyph_bitmap(&glyph,bitmap)==bitmap);
    unsigned opaque=0;for(unsigned y=0;y<glyph.box_h;++y) for(unsigned x=0;x<glyph.box_w;++x) opaque+=bitmap->data[y*bitmap->header.stride+x]!=0;
    assert(opaque>0&&opaque<glyph.box_w*glyph.box_h);lv_draw_buf_destroy(bitmap);
    assert(lv_font_get_glyph_dsc(tetris_font(),&glyph,0x00e9,0)); // Spanish fallback
    // Maximum scores still fit the compact HUD without touching the preview.
    auto* metric=lv_label_create(screen);lv_obj_set_style_text_font(metric,&font_rajdhani_20,0);
    lv_label_set_text(metric,"PUNTOS 4294967295");lv_obj_update_layout(metric);assert(lv_obj_get_width(metric)<=210);
    lv_label_set_text(metric,"RÉCORD 4294967295");lv_obj_update_layout(metric);assert(lv_obj_get_width(metric)<=210);
    lv_obj_set_style_text_line_space(metric,0,0);
    lv_label_set_text(metric,"NIVEL 100001\nLÍNEAS 1000000");lv_obj_update_layout(metric);
    assert(lv_obj_get_width(metric)<=150&&lv_obj_get_height(metric)<=42);
    auto* board=lv_obj_create(screen);lv_obj_set_pos(board,0,header_height);lv_obj_set_size(board,board_width,board_height);
    lv_obj_set_style_pad_all(board,0,0);lv_obj_set_style_border_width(board,0,0);
    lv_obj_set_style_radius(board,0,0);lv_obj_remove_flag(board,LV_OBJ_FLAG_SCROLLABLE);
    TetrisRenderer renderer;renderer.init(board,cell_width,TETRIS_COLUMNS);TetrisEngine game;game.start(123);
    const auto objects=lv_obj_get_child_count(board);assert(objects==TETRIS_VISIBLE_CELLS);
    for(int frame=0;frame<300;++frame) {
        if(game.snapshot().state==TetrisState::Finished) game.start(frame+1);
        if(frame%5==0) game.rotate();
        game.advance(30);renderer.draw(game.snapshot());lv_tick_inc(30);lv_timer_handler();
        assert(lv_obj_get_child_count(board)==objects);
    }
    lv_obj_update_layout(board);lv_area_t bounds{};lv_obj_get_coords(board,&bounds);
    assert(bounds.x1==0&&bounds.x2==screen_width-1);
    assert(bounds.y1==header_height&&bounds.y2==screen_height-1);
    for(unsigned i=0;i<TETRIS_VISIBLE_CELLS;++i) {
        lv_area_t cell{};lv_obj_get_coords(lv_obj_get_child(board,i),&cell);
        assert(cell.x1==int(i%TETRIS_COLUMNS)*cell_width&&cell.x2==cell.x1+cell_width-1);
        assert(cell.y1==header_height+int(i/TETRIS_COLUMNS)*cell_height&&cell.y2==cell.y1+cell_height-1);
        assert(cell.x2-cell.x1==cell.y2-cell.y1);
        assert(cell.x1>=bounds.x1&&cell.x2<=bounds.x2&&cell.y1>=bounds.y1&&cell.y2<=bounds.y2);
    }
    assert(flushed>0);
    // Continued 10-column saves also use square cells, centered, not stretched.
    auto* classic=lv_obj_create(screen);lv_obj_set_pos(classic,board_x(10),header_height);
    lv_obj_set_size(classic,10*cell_width,board_height);lv_obj_set_style_pad_all(classic,0,0);
    lv_obj_set_style_border_width(classic,0,0);lv_obj_set_style_radius(classic,0,0);
    lv_obj_remove_flag(classic,LV_OBJ_FLAG_SCROLLABLE);
    TetrisRenderer classic_renderer;classic_renderer.init(classic,cell_width,10);
    TetrisSnapshot saved;saved.columns=10;saved.state=TetrisState::Paused;saved.piece={1,0,8,8};
    classic_renderer.draw(saved);lv_obj_update_layout(classic);
    assert(lv_obj_get_child_count(classic)==200);
    lv_area_t last{};lv_obj_get_coords(lv_obj_get_child(classic,199),&last);
    assert(last.x2==659&&last.y2==1279&&last.x2-last.x1==last.y2-last.y1);
    lv_obj_clean(screen);lv_deinit();
    std::printf("LVGL: 240 cuadrados de 60 px, ancho completo, compatibilidad clásica, HUD compacto y %u flushes OK\n",flushed);
}
