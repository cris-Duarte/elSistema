# Deuda visual

Registro visible de incumplimientos de [`lineamientos.md`](lineamientos.md)
detectados en el código existente. Una entrada se cierra solo cuando el elemento
cumple el lineamiento **por completo**; no se deja UI "a medio migrar".

Última auditoría: **iteración 2 — sistema tipográfico**.

---

## Cerrado en la iteración 1

Iteración de identidad: se estableció la capa de tokens y se corrigieron los
incumplimientos triviales que aparecieron en la auditoría.

| # | Incumplía | Lineamiento | Cómo quedó |
|---|---|---|---|
| 1 | Los 11 colores base estaban fuera de la paleta (`0x6CE5FF` de cian, `0xF4F7FF` de texto, superficies `0x0C1122`/`0x0F1730`…) | §4 Paleta | `main/theme.hpp` es la única fuente de verdad; `ui.cpp` solo define alias cortos sobre esos tokens |
| 2 | El éxito era verde menta (`0x7FFFC6`) y el aviso, ámbar | §4 «Éxito: cian brillante; dorado puntual solo en logros mayores» | El éxito es cian; el dorado queda reservado al rango S y a la celebración de misión completada |
| 3 | La escala de rango E–S incluía un verde (`D = 0x76D7B1`) y colores ajenos a la paleta | §4 Paleta | Escala derivada: E apagado → D cian apagado → C cian → B azul‑violeta → A violeta → S dorado (`theme::RankE..RankS`) |
| 4 | 20 colores literales dispersos por el archivo (`0x17384C`, `0x321727`, `0x20365A`, `0x183D38`, `0x152B43`, `0x273653`, `0x253451`, `0x080D1C`, `0x111B34`, `0x2B4169`…) | §2 «no crear variantes; extender y normalizar los existentes» | Tokens semánticos: `AccentSoft`, `DangerSoft`, `SurfaceAlt`, `Border`, `Scrim` |
| 5 | Cuerpo por debajo del mínimo: `font_es_14` en 10 sitios (rótulos de botón, subtítulo de cabecera, aviso de almacenamiento, estado del cronómetro, tiempo transcurrido, recordatorio, detalle del historial, ayudas del reloj) | §4 «Cuerpo mínimo ~16 px» | Todo a 16 px. `font_es_14` quedó sin referencias y el enlazador la descarta (binario −7 KB) |
| 6 | Área táctil menor de 48 px: botones de fila de misión a **38 px** (10 botones), historial a 46 px (2), filas de rueda a 38 px | §4 y §9 | Botones de fila a 48 px, historial a 48 px, filas de rueda a 51 px (35 de línea + 16 de espaciado) |
| 7 | Radios de 5–6 px | §4 «esquinas redondeadas moderadas (8–12 px)» | `theme::RadiusControl` = 8 (controles) y `theme::RadiusPanel` = 10 (paneles) |
| 8 | El botón «← VOLVER» usaba `U+2190`, fuera del rango de las fuentes embebidas: se dibujaba en blanco | §2 «reutilizar assets ya embebidos» | Texto plano: «VOLVER» |

Verificación de la iteración: compila sin advertencias propias y arranca en el
Tab5 sin *panic*. **Pendiente de confirmación visual del usuario.**

---

## Cerrado en la iteración 2 — sistema tipográfico

Montserrat cubría con una sola voz los dos papeles que el §4 separa: fuente
geométrica/futurista para los elementos del Sistema y fuente legible para el texto
corrido. Se introdujeron Rajdhani SemiBold y Poppins Regular con procedencia
verificable.

| # | Incumplía | Lineamiento | Cómo quedó |
|---|---|---|---|
| 9 | Una sola familia para chrome y contenido: no se distinguía «el Sistema habla» de «el usuario escribió» | §4 Tipografía y §3 «consistencia total» | Rajdhani para títulos, rótulos, cifras, teclas, rango y nombres de misión; Poppins para descripciones, notas y campos. Regla y procedencia en `fonts/README.md` |
| 10 | Assets de fuente sin uso (`font_es_14`) y cuatro fuentes Montserrat que la nueva escala dejaba obsoletas | §2 «no dejar UI a medio migrar» | Se eliminaron `font_es_14/16/20/28`; el build usa `font_poppins_16`, `font_rajdhani_20/28/40`, generadas por `tools/fonts/generar_fuentes.sh` con SHA256 fijado |
| 11 | Las filas de las ruedas del reloj quedaban en 46 px al cambiar de fuente (Rajdhani 28 tiene 30 px de línea frente a 35 de Montserrat) | §4 y §9 «táctil ≥ 48 px» | Espaciado de línea a 20 → paso de 50 px, y comprobado antes de flashear |

Efecto en el binario: 959 KB → **945 KB**, pese a añadir una familia nueva.

---

## Deuda abierta

### Prioridad alta

1. **Ventana del Sistema** (§5). `render_overlay()` dibuja un panel genérico con
   título y cuerpo. Falta el componente único con marco cian fino y encabezado
   `[ SISTEMA ]` / `⚠ ALERTA`. Afecta a todos los diálogos: tutorial, recompensa,
   recordatorio, confirmación de borrado, detalle de misión, avisos y pantalla
   apagada. Es la pieza con más impacto en la identidad.

2. **Barra de progreso de XP y nivel** (§5). La cabecera muestra solo texto
   (`NIVEL n   xp / total XP`). Falta la barra delgada con pulso de luz al llenarse
   y feedback visible al ganar XP.

3. **Codificación de estado en las filas de misión** (§5). El estado se comunica
   sobre todo por color; falta añadir **icono + etiqueta de texto** para que no
   dependa solo del color.

4. **Movimiento** (§6). La UI se reconstruye entera en cada acción
   (`lv_obj_clean` + render completo), así que no hay transiciones ni animaciones:
   la celebración de misión completada es un diálogo estático. Requiere decidir
   antes la estrategia (animar el cambio sin reconstruir) porque condiciona el
   resto de animaciones.

### Prioridad media

5. **Estados vacío / carga / error / éxito** (§5). Existen mensajes vacíos
   («La cola está vacía.», «No hay misiones pendientes.») sin diseño propio ni el
   tono del lineamiento; no hay estado de carga ni diseño específico de error (los
   errores caen en una ventana de información).

6. **Feedback en <100 ms** (§7). Sin medir. El re-render completo puede acercarse
   al límite en el historial cuando hay muchas filas.

7. **Editor solo horizontal.** La fila de RANGO y RECORDATORIO llega hasta x=906;
   en vertical (720 px de ancho) se sale de pantalla. Deuda visual y funcional.

8. **Estado presionado** (§7). Existe en los botones (violeta al pulsar), pero no
   hay aumento de luminosidad o escala, y el teclado y las ruedas no lo tienen
   definido.

9. **Jerarquía de la cabecera.** El subtítulo «MÓDULO DE PRODUCTIVIDAD» y el aviso
   de almacenamiento compiten con el nivel; conviene revisarla junto con la barra
   de XP.

### Prioridad baja

10. ~~`font_es_14.c` sin uso~~ — **cerrado en la iteración 2**: se eliminaron las
    cuatro fuentes Montserrat junto con su entrada en `CMakeLists.txt`.

11. `CONFIG_LV_USE_KEYBOARD` ya no se usa: el teclado es propio con
    `lv_buttonmatrix`. Queda como bandera de configuración sobrante.

12. Esquinas cortadas tipo HUD para nivel y rango (§4): no implementadas; son
    opcionales, no un requisito.

13. Scanlines de fondo (§4): no implementados; el lineamiento los admite solo como
    micro‑detalle estático.

14. **Recortes de texto conservadores (nueva, iteración 2).** `truncate()` corta los
    títulos de misión a 32 caracteres en horizontal y 50 en vertical. Rajdhani es
    más estrecha que la Montserrat anterior, así que ahora caben más: se puede
    subir el límite para mostrar títulos completos.

15. **Textos de cuerpo más anchos (nueva, iteración 2).** Poppins 16 ocupa más
    ancho que Montserrat 16 al mismo tamaño, de modo que los textos largos (nota
    del reloj, descripciones, cuerpo del tutorial) pueden pasar a una línea más.
    Requiere la revisión visual del usuario.
