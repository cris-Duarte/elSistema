#include "assistant_character.h"

extern const unsigned char assistant_character_map[];
extern const unsigned int assistant_character_map_len;

const lv_image_dsc_t assistant_character = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .flags = 0,
        .w = 320,
        .h = 320,
        .stride = 1280,
        .reserved_2 = 0,
    },
    .data_size = 320 * 320 * 4,
    .data = assistant_character_map,
    .reserved = 0,
};
