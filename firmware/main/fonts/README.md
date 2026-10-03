# Fuentes embebidas

Generadas con `lv_font_conv` 1.5.3 mediante `tools/fonts/generar_fuentes.sh`, que
descarga los TTF de Google Fonts verificándolos por SHA256 y aplica siempre las
mismas opciones. Los TTF no se versionan; las fuentes generadas (`.c`) sí, de modo
que la procedencia queda fijada y el resultado es reproducible.

| Archivo | Fuente | px | Papel |
|---|---|---|---|
| `font_poppins_16.c` | Poppins Regular | 16 | contenido del usuario y texto corrido |
| `font_rajdhani_20.c` | Rajdhani SemiBold | 20 | rótulos, botones, teclas, rango |
| `font_rajdhani_28.c` | Rajdhani SemiBold | 28 | títulos de pantalla y de misión, ruedas |
| `font_rajdhani_40.c` | Rajdhani SemiBold | 40 | títulos protagonistas y nivel |
| `font_rajdhani_64.c` | Rajdhani SemiBold | 64 | cronómetro de la misión activa |
| `font_rajdhani_128.c` | Rajdhani SemiBold | 128 | cronómetro principal de la misión activa |

## Regla de asignación

Lo que dice el Sistema —títulos, rótulos, cifras, acciones y **nombres de
misión**— va en **Rajdhani**. El texto corrido que escribe el usuario
—descripciones, notas— va en **Poppins**. Así el HUD tiene voz propia y el
contenido conserva la máxima legibilidad.

En el código los sitios de uso no nombran la fuente directamente, sino un alias
de rol definido junto a los tokens de color en `ui.cpp` (`FONT_BODY`,
`FONT_LABEL`, `FONT_DISPLAY`, `FONT_NUMBER`).

## Cobertura y opciones

Cada archivo cubre el rango `U+0020–U+00FF`: ASCII, acentos, diéresis, eñe y
signos de apertura. Verificado con fontTools sobre los TTF: ambas familias cubren
todo lo que la app usa, y lo único ausente son los controles C1
(`U+007F–U+009F`), que no son imprimibles. Rajdhani incluye además `¿` y `¡`.

Opciones de generación: `--bpp 4 --no-compress --no-prefilter -r 0x20-0xFF
--force-fast-kern-format`. Se mantiene sin comprimir aunque
`CONFIG_LV_USE_FONT_COMPRESSED` esté activo: cuesta algo más de flash (que sobra)
y evita trabajo de decodificación en cada frame.

## Símbolos que no están

**Ninguna de estas fuentes incluye el área `U+F000+`** que LVGL usa para
`LV_SYMBOL_BACKSPACE`, `LV_SYMBOL_NEW_LINE`, `LV_SYMBOL_OK`, etc.: esos glifos
saldrían en blanco. Por eso el teclado virtual no usa el widget de LVGL, sino uno
propio con `lv_buttonmatrix` y etiquetas de texto —`MAYÚS`, `BORRAR`, `ESPACIO`,
`INTRO`— que sí caben. Por el mismo motivo se evitó el carácter `←` (U+2190) en
los botones.

## Licencia

Rajdhani y Poppins se distribuyen bajo **SIL Open Font License 1.1**:

- `OFL-Rajdhani.txt` — Copyright 2014, Indian Type Foundry.
- `OFL-Poppins.txt` — Copyright 2020, The Poppins Project Authors.

Las fuentes anteriores (Montserrat, `font_es_14/16/20/28`) quedaron sustituidas en
la iteración 2 del plan de UI; siguen recuperables del historial de git.
