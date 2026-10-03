#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fuentes embebidas. Se generan con tools/fonts/generar_fuentes.sh a partir de
// Rajdhani SemiBold (voz del Sistema) y Poppins Regular (contenido del usuario).
// Ver fonts/README.md para procedencia, opciones y licencia.
//
// Regla de asignacion: lo que dice el Sistema --titulos, rotulos, cifras,
// acciones y nombres de mision-- va en Rajdhani; el texto corrido que escribe el
// usuario va en Poppins.
extern const lv_font_t font_poppins_16;   // contenido, descripciones, campos
extern const lv_font_t font_rajdhani_20;  // rotulos, botones, teclas, rango
extern const lv_font_t font_rajdhani_28;  // titulos de pantalla y de mision
extern const lv_font_t font_rajdhani_40;  // cifras grandes: cronometro, nivel
extern const lv_font_t font_rajdhani_64;  // cronometro protagonista
extern const lv_font_t font_rajdhani_128; // cronometro principal

#ifdef __cplusplus
}
#endif
