# Plan de iteraciones — deuda visual

Plan de resolución de las 13 entradas de [`deuda-visual.md`](deuda-visual.md)
bajo [`lineamientos.md`](lineamientos.md).

---

## Decisiones confirmadas

| Decisión | Resuelto |
|---|---|
| Escala tipográfica | Rajdhani para el Sistema, Poppins para el texto corrido |
| Títulos de misión | **Rajdhani**, como el Sistema (también los de las listas) |
| Barra de XP | Bajo el nivel, como elemento propio; el número de nivel manda |
| Orden de trabajo | Iteración 2 (tipografía) primero |

> **Nota sobre la escala.** Al pasar los títulos de misión a Rajdhani, Poppins 22
> se quedó sin rol: se había propuesto solo para eso. Se descarta esa variante para
> no crear un asset sin uso —que es justo la deuda 10, `font_es_14`— y la escala
> queda en **4 variantes** (≈ 528 KB en lugar de 655 KB). Recuperarla para la
> descripción de la misión activa es una línea del script de generación.

---

## Principio de orden

**Tipografía → componentes → contenido y estados → movimiento → limpieza.**

El motivo no es estético sino práctico: cada iteración posterior coloca texto, así
que cambiar la tipografía más adelante obligaría a reajustar todos los layouts una
segunda vez. Por eso la fuente va primero, aunque no sea la deuda más visible.

| Iteración | Tema | Deuda que cierra |
|---|---|---|
| 1 ✅ | Identidad: color y tipografía base | 1–8 del registro |
| **2** | Sistema tipográfico (Rajdhani + Poppins) | Decisión de fuentes del §2 |
| **3** | Ventana del Sistema | Alta 1 (+ parte de media 5) |
| **4** | Progreso: barra de XP y jerarquía de cabecera | Alta 2, media 9 |
| **5** | Listas y estados | Alta 3, media 5 |
| **6** | Movimiento y feedback | Alta 4, media 6, 8 |
| **7** | Limpieza y detalles | Baja 10–13, media 7 |

---

## Iteración 2 — Sistema tipográfico

### Decisión (necesidad documentada, §2)

El lineamiento pide *fuente geométrica/futurista para elementos del Sistema* y
*texto corrido en fuente legible de buen peso*. Montserrat cubría ambos roles con
la misma voz, así que no había contraste entre "el Sistema habla" y "el usuario
escribió". Se introducen dos familias:

- **Rajdhani SemiBold** — voz del Sistema: títulos (incluidos los de misión),
  rótulos, cifras, teclas, rango. Geométrica, condensada y de esquinas rectas:
  encaja con el HUD.
- **Poppins Regular** — contenido y texto corrido: descripciones, notas, campos,
  ayudas. Geométrica pero de trazo abierto y x‑alta grande, muy legible en 5".

### Viabilidad verificada (no estimada)

| Comprobación | Resultado |
|---|---|
| `lv_font_conv` en el registro npm | **1.5.3**, la misma versión con que se generaron las actuales |
| TTF de Rajdhani SemiBold/Bold y Poppins Regular/Medium | accesibles (HTTP 200, Google Fonts) |
| Cobertura Latin‑1 de las cuatro | **completa**; solo faltan los controles C1 (U+007F–U+009F), no imprimibles |
| `¿` y `¡` en Rajdhani | presentes |
| Generación de muestra de las variantes | sin avisos ni glifos faltantes |
| Opciones originales | recuperadas de la cabecera de `font_es_16.c`: `--bpp 4 --no-compress --no-prefilter -r 0x20-0xFF --force-fast-kern-format` |

Se mantiene `--no-compress` aunque `CONFIG_LV_USE_FONT_COMPRESSED` esté activo:
cuesta algo más de flash (que sobra) y evita trabajo de decodificación en cada
frame, que es lo que el §6 protege.

### Escala final

| Token | Fuente | px | Uso |
|---|---|---|---|
| `font_body_16` | Poppins Regular | 16 | descripciones, notas, campos, ayudas, cuerpo de diálogos |
| `font_label_20` | Rajdhani SemiBold | 20 | rótulos y secciones, botones, teclas, rango, cifras pequeñas, **títulos de misión en listas** |
| `font_title_28` | Rajdhani SemiBold | 28 | títulos de pantalla, **título de la misión activa**, ventana del Sistema |
| `font_number_40` | Rajdhani SemiBold | 40 | cronómetro activo, nivel |

Regla de asignación: **lo que dice el Sistema —rótulos, cifras, acciones y nombres
de misión— va en Rajdhani; el texto corrido que escribe el usuario va en Poppins.**

### Costo real en flash (medido con las muestras)

| Asset | Generado |
|---|---|
| Poppins Regular 16 | 78,8 KB |
| Rajdhani SemiBold 20 | 79,7 KB |
| Rajdhani SemiBold 28 | 128,8 KB |
| Rajdhani SemiBold 40 | 241,2 KB |
| **Total (4)** | **≈ 528 KB** |

Sustituye a los 531 KB actuales (4 fuentes Montserrat), así que el binario se
mantiene en torno a 960 KB aunque se añada la barra de XP de la iteración 4. La
partida más cara es `font_number_40`; si algún día aprieta el flash, basta con
reutilizar la de 28.

### Mapa de sustitución (sitio por sitio, no reemplazo ciego)

`font_es_28` se usa en 10 sitios con dos roles distintos, y `font_es_20` en 5:

| Sitio actual | Hoy | Pasa a |
|---|---|---|
| Marca «EL SISTEMA» (cabecera) | 28 | `font_title_28` |
| Título de pantalla (editor, reloj, historial) | 28 | `font_title_28` |
| Encabezado de la ventana del Sistema | 28 | `font_title_28` |
| «NINGUNA MISIÓN EN CURSO» (estado vacío) | 28 | `font_title_28` |
| **Título de la misión activa** | 28 | `font_title_28` |
| Cronómetro activo *(cifra)* | 28 | `font_number_40` |
| Valores de las ruedas del reloj | 28 | `font_label_20` |
| Rango E–S (panel activo y filas) | 20 | `font_label_20` |
| Desfase horario «UTC−3:00» | 20 | `font_label_20` |
| Teclas del teclado | 20 | `font_label_20` |
| Estado vacío del historial | 20 | `font_body_16` |
| **Título de misión en filas de lista** | 16 | `font_label_20` |
| Rótulos de botón (`button()`) | 16 | `font_label_20` |
| Descripciones, campos, notas, ayudas | 16 | `font_body_16` |

### Ajustes obligados por la métrica

Cambiar de fuente mueve las alturas de línea, y eso rompe dos cosas ya validadas:

1. **Ruedas del reloj.** Rajdhani 28 tiene **30 px de línea** frente a los 35 de
   Montserrat 28. Con el espaciado actual (16) la fila quedaría en **46 px**, por
   debajo del mínimo táctil. Hay que subir `kClockRollerLineSpace` a **20**
   (30 + 20 = 50 px de paso) y recalcular la altura de la rueda.
2. **Recorte de títulos en listas.** Rajdhani es más estrecha que Montserrat, así
   que los límites de `truncate()` (32 caracteres en horizontal, 50 en vertical)
   dejan de ser ajustados: conviene **bajarlos** o eliminarlos, ahora que caben más
   caracteres. El cuerpo en Poppins, en cambio, es más ancho que Montserrat y puede
   empujar textos largos de una a dos líneas.

### Entregables

- `tools/fonts/generar_fuentes.sh` — descarga los TTF con URL y SHA256 fijados,
  instala `lv_font_conv` y regenera los 4 assets. Deja la procedencia trazable; los
  TTF no se versionan (≈ 1 MB de binarios) pero su hash sí.
- `firmware/main/fonts/font_{body_16,label_20,title_28,number_40}.c`.
- `firmware/main/fonts/fonts.h` con los nombres semánticos; alias cortos en
  `ui.cpp` junto a los de color, para que cambiar de fuente sea de una línea.
- `firmware/main/fonts/OFL.txt` — Rajdhani y Poppins son SIL OFL 1.1.
- `firmware/main/CMakeLists.txt` — alta de los 4 assets y **baja de
  `font_es_14.c`** (deuda 10, sin uso desde la iteración 1).
- `fonts/README.md` — procedencia, opciones, licencia y regla de asignación.
- `lineamientos.md` §2 — registrar la necesidad documentada de fuentes nuevas.

### Riesgos

- **No puedo ver la pantalla.** El salto de Montserrat a Poppins cambia anchuras y
  densidades en todas las pantallas. Es previsible una ronda de ajustes finos tras
  tu revisión.
- Los textos largos en Poppins 16 pueden pasar de una a dos líneas y desplazar lo
  que tienen debajo.
- El cronómetro y el nivel pasan de 28 a 40 px: hay que revisar que no choquen con
  la descripción en el panel activo.

### Criterio de cierre

Compila sin advertencias, arranca sin *panic*, y en pantalla se distingue la voz
del Sistema de la del contenido; los elementos táctiles siguen ≥ 48 px; ninguna
etiqueta queda cortada ni desbordada.

---

## Iteración 3 — Ventana del Sistema

**Cierra:** deuda alta 1 y parte de la media 5.

Un único componente `system_window(...)` sustituye al `render_overlay()` genérico:

- Marco cian fino (1–2 px) con brillo interior simulado (borde interno más claro).
- Encabezado con variantes: `[ SISTEMA ]`, `[ AVISO ]`, `[ RECOMPENSA ]`,
  `⚠ ALERTA` (para error y borrado).
- Esquinas cortadas tipo HUD **solo** aquí y en las tarjetas de nivel y rango (§4).
- Entrada/salida consistente: desde el centro con escala sutil (ver iteración 6).
- Migran los 7 usos: tutorial, recompensa, recordatorio, confirmar borrado, detalle
  de misión, avisos y pantalla apagada.

**Riesgo:** el tutorial y el detalle de misión son modales con contenido variable;
el componente debe aceptar cuerpo arbitrario y hasta 3 acciones sin romper el ritmo
vertical.

---

## Iteración 4 — Progreso: barra de XP y jerarquía de cabecera

**Cierra:** deuda alta 2 y media 9.

- Barra fina de XP **bajo el nivel**, con relleno y pulso de luz al llenarse.
- Nivel como cifra en `font_number_40`; el número manda y el texto `NIVEL n
  xp/total XP` deja de ser protagonista.
- Se revisa la jerarquía de la cabecera: marca, nivel/XP, acciones y aviso de
  almacenamiento dejan de competir.
- Feedback visible al ganar XP (§5): relleno animado + cifra flotante.

**Depende de:** iteración 2.

---

## Iteración 5 — Listas y estados

**Cierra:** deuda alta 3 y media 5.

- Filas de misión: separadores tenues y **dificultad y estado con color + icono +
  etiqueta de texto** (nunca solo color).
- Estados con diseño propio y el tono del Sistema:
  - vacío: «No hay misiones activas — el Sistema te espera»
  - error: variante `⚠ ALERTA` de la ventana del Sistema
  - éxito: confirmación breve con la firma visual de recompensa
  - carga: hoy no aplica (todo es local); se define el patrón por si aparece.

**Depende de:** iteraciones 3 y 2.

---

## Iteración 6 — Movimiento y feedback

**Cierra:** deuda alta 4, media 6 y 8.

Requiere una decisión previa: la UI se reconstruye entera en cada acción
(`lv_obj_clean` + render completo), así que no hay objetos estables que animar.
Opciones evaluadas:

| Opción | Coste | Veredicto |
|---|---|---|
| A. Reescribir el render para actualizar en sitio | Alto y arriesgado | Descartada por ahora |
| B. Transición de pantalla sobre el contenedor | Bajo | **Recomendada** como base (200–300 ms, opacidad/posición) |
| C. Animar componentes concretos tras el render | Medio | **Recomendada** para recompensa y barra de XP |

Plan: **B + C**. Se mide el tiempo de render para verificar el umbral de <100 ms del
§7 y se añade estado presionado con aumento de luminosidad en botones, teclado y
ruedas. Cualquier animación que comprometa los 60 fps se simplifica (§6).

---

## Iteración 7 — Limpieza y detalles

**Cierra:** baja 10–13 y media 7.

- Retirar `font_es_14.c` del build (ya sin uso) y el `LV_USE_KEYBOARD` huérfano.
- Editor adaptable a vertical: la fila de RANGO/RECORDATORIO llega a x=906 y se sale
  en 720 px de ancho.
- Opcionales del §4, solo si aportan: esquinas HUD y micro‑scanlines de fondo.
