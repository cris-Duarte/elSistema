#pragma once
#include "tetris_model.hpp"
namespace el_sistema::tetris_layout {
// Twelve square columns and twenty rows exactly fit the portrait display.
inline constexpr int screen_width = 720, screen_height = 1280;
inline constexpr int header_height = 80;
inline constexpr int cell_width = screen_width / TETRIS_COLUMNS;
inline constexpr int cell_height = cell_width;
inline constexpr int board_width = cell_width * TETRIS_COLUMNS;
inline constexpr int board_height = cell_height * TETRIS_VISIBLE_ROWS;
// A restored classic session is centered without stretching or changing rules.
constexpr int board_x(int columns) { return (screen_width-columns*cell_width)/2; }
inline constexpr int preview_x = 400, preview_y = 34, preview_cell = 18;
inline constexpr int pause_x = 616, pause_y = 12, pause_width = 92, pause_height = 52;
static_assert(cell_width == cell_height);
static_assert(board_width == screen_width);
static_assert(header_height + board_height == screen_height);
static_assert(preview_x + 4 * preview_cell <= screen_width);
static_assert(preview_y + 2 * preview_cell <= header_height);
static_assert(pause_width >= 48 && pause_height >= 48);
}
