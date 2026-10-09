#pragma once

#include <algorithm>

namespace el_sistema {

// Native Tab5 layouts. The disk uses the entire shorter screen dimension;
// navigation and measurements stay outside its square, never over its grid.
struct RadarLayout {
    bool portrait;
    int diameter;
    int controls_x, controls_width;
    int title_y, filters_y, actions_y, legend_y, status_y;
    int detail_y, detail_height, list_y, list_height, pager_y;
};

inline RadarLayout radar_layout(int width, int height)
{
    const bool portrait = height > width;
    const int diameter = std::min(width, height);
    const int x = portrait ? 24 : diameter + 24;
    return {portrait, diameter, x, width-x-24,
            portrait ? diameter+8 : 20,
            portrait ? diameter+64 : 84,
            portrait ? diameter+64 : 148,
            portrait ? diameter+120 : 212,
            portrait ? diameter+172 : 268,
            portrait ? diameter+208 : 324,
            portrait ? 176 : 224,
            portrait ? diameter+396 : 560,
            portrait ? 100 : 88,
            height-56};
}

} // namespace el_sistema
