# Fuentes en español

Fuentes generadas con `lv_font_conv` 1.5.3 a partir de
`Montserrat-Medium.ttf`, distribuida con las herramientas de fuentes de LVGL.

Cada archivo incluye el rango Unicode `U+0020–U+00FF`, que cubre ASCII,
acentos, diéresis, eñe, signos de apertura y el resto de Latin-1.

**Estas fuentes no incluyen el área de símbolos `U+F000+`** que LVGL usa para
`LV_SYMBOL_BACKSPACE`, `LV_SYMBOL_NEW_LINE`, `LV_SYMBOL_OK`, etc. Cualquier
widget que muestre esos símbolos —el teclado de LVGL, por ejemplo— los dibujaría
en blanco. Por eso el teclado del editor usa `lv_font_montserrat_16`, la fuente
integrada de LVGL, que sí los trae; las fuentes `font_es_*` se reservan para el
texto del usuario, donde las tildes y la eñe sí importan.
