# Actualización — sesión de trabajo

**Proyecto:** El Sistema v0.2.0 — firmware para M5Stack Tab5 (ESP32‑P4)
**Rama:** `main` · **Plataforma:** ESP‑IDF 5.4.2 + LVGL 9.2.2

Este documento describe los cambios realizados en esta sesión, los defectos que
aparecieron por el camino (varios de ellos preexistentes y latentes), las
verificaciones hechas sobre el dispositivo real y lo que queda pendiente.

---

## Resumen

La sesión empezó familiarizándose con el proyecto y terminó tocando cinco áreas:

| Área | Estado |
|---|---|
| Pruebas de dominio conectadas al build | Terminado y verificado en el Tab5 (4/4) |
| Pantalla de fecha y hora rediseñada con ruedas | Terminado, verificado por el usuario |
| Teclado virtual en español | Terminado, verificado por el usuario |
| Rotación automática corregida | Verticales confirmadas; horizontales corregidas a falta de confirmación |
| Botón físico y apagado de pantalla | Implementado en GPIO35; el botón visible resultó inalcanzable |
| Dashboard horizontal (columna derecha vacía) | Terminado y flasheado |

---

## 1. Pruebas de dominio conectadas al build

Las pruebas de `main/test/test_domain.cpp` existían pero eran **completamente
inertes**. Se encontraron cuatro defectos encadenados, y cada uno solo se
manifestaba al resolver el anterior.

### 1.1 No entraban en el build

`firmware/CMakeLists.txt` no declaraba `EXTRA_COMPONENT_DIRS`, así que
`main/test/` no se compilaba nunca (el `build.ninja` no contenía ni una
referencia al archivo).

**Solución:** opción de CMake `EL_SISTEMA_TESTS` (por defecto `OFF`) que añade el
componente y define la bandera de compilación `EL_SISTEMA_RUN_TESTS`.

### 1.2 `unity` no era dependencia de `main`

Con las pruebas activadas, el enlace fallaba. Aquí apareció algo poco obvio:
**`main/CMakeLists.txt` se procesa dos veces** por parte de IDF. En la primera
pasada —la expansión de requisitos del *component manager*, de la que se derivan
los *include dirs* y la validación— la variable de la opción **todavía no existe**,
así que una dependencia condicional se perdía.

**Solución:** `unity` figura siempre, y en `PRIV_REQUIRES` porque solo lo usa
`app_main.cpp` (no forma parte de la interfaz pública del componente).

### 1.3 El runner veía cero pruebas

`TEST_CASE` solo aporta funciones constructoras que nadie referencia, así que el
enlazador **no extraía el objeto de la librería estática** y el registro quedaba
vacío. El síntoma era engañoso: las pruebas "se ejecutaban" en 7 ms sin imprimir
nada.

**Solución:** `WHOLE_ARCHIVE` en el componente de pruebas.

### 1.4 El assert de 64 bits fallaba por diseño

`TEST_ASSERT_EQUAL_UINT64` fallaba aunque los valores fueran correctos. En
`unity_internals.h`, cuando `UNITY_SUPPORT_64` no está definido:

```c
#define UNITY_TEST_ASSERT_EQUAL_UINT64(...)  UNITY_TEST_FAIL(..., UnityStrErr64)
```

Es decir, **falla siempre**, sin mirar los valores. Como ese assert es el tercero
del test y Unity aborta en el primero que falla, que fallara demuestra que los dos
anteriores (`level == 4` y `level_xp == 0`) sí pasaron: la lógica de XP era
correcta y solo faltaba configuración.

**Solución:** `CONFIG_UNITY_ENABLE_64BIT=y` en `sdkconfig` y `sdkconfig.defaults`.

### 1.5 La salida de Unity no llega por USB

Detalle de plataforma que costó un ciclo: la **consola primaria es UART0**, y
Unity escribe con `unity_putc()` por la **UART del ROM**, que en el Tab5 no está
expuesta por USB (el USB es consola *secundaria*). Las pruebas se ejecutaban pero
su salida era invisible.

**Solución:** el arranque de pruebas del firmware enumera el registro y ejecuta
cada prueba, informando el resultado con `ESP_LOGI`, que sí viaja por USB:

```
I (1125) el_sistema: Pruebas de dominio registradas: 4
I (1155) el_sistema:   XP aplica bases, minutos completos y límite temporal      OK
I (1174) el_sistema:   una recompensa puede subir varios niveles                  OK
I (1198) el_sistema:   la búsqueda ignora mayúsculas y acentos españoles       OK
I (1219) el_sistema:   el modelo localiza misiones por identificador estable      OK
I (1220) el_sistema: Resultado de pruebas: 4 ejecutadas, 0 fallidas
```

Con `-DEL_SISTEMA_TESTS=OFF` (por defecto) el componente `test` no entra en el
build y **el binario mide exactamente lo mismo** que antes: 966 624 bytes.

---

## 2. Pantalla de fecha y hora: de teclado numérico a ruedas

La pantalla original pedía teclear `DDMMAAAA` y `HHMM` en dos campos con un
teclado numérico en pantalla (12 dígitos) más la zona horaria con `−`/`+`.

### 2.1 Dos defectos encontrados

1. **Cinco teclas del teclado numérico salían en blanco.** El mapa de LVGL para
   modo número es:

   ```c
   "1","2","3", LV_SYMBOL_KEYBOARD,
   "4","5","6", LV_SYMBOL_OK,
   "7","8","9", LV_SYMBOL_BACKSPACE,
   "+/-","0",".", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT
   ```

   y las fuentes del proyecto cubren solo `U+0020–U+00FF`, sin el área de símbolos
   `U+F000+`. De 16 teclas, 5 eran informes. Eso explica el "no responde o es
   difícil acertarle".

2. **La pantalla no conservaba lo tecleado.** Los campos se creaban vacíos en cada
   `render()`, así que pulsar `−`/`+` de la zona horaria **borraba la fecha ya
   escrita**.

### 2.2 Diseño nuevo

Cinco ruedas deslizables (`lv_roller`), sin teclado:

```
CONFIGURAR FECHA Y HORA                     Zona: America/Asuncion
  DÍA     MES        AÑO     HORA   MINUTO
 [ 1 ] [ Octubre ] [ 2026 ] [ 12 ] [ 25 ]
 ZONA HORARIA
 [-] UTC-3:00 [+]  Pasos de 30 min      [ GUARDAR Y CONTINUAR ]
```

- **Valor inicial útil:** la fecha de compilación del firmware (`__DATE__`/
  `__TIME__`), que está a minutos del flasheo, en lugar de 12 dígitos a ciegas.
- **La rueda de días se recalcula** según mes y año, con bisiestos: febrero ofrece
  28 o 29, nunca 31. Se evita reconstruirla si el mes no cambia de longitud.
- **Las selecciones sobreviven a los re‑render** (`capture_clock()`), lo que
  corrige el borrado descrito arriba.
- Se quitó la etiqueta `Valor inicial: America/Asuncion`, que no cambiaba nunca
  aunque movieras el desfase; ahora la zona aparece en la cabecera.

---

## 3. Teclado virtual en español

### 3.1 El conflicto que decidió el diseño

| Fuente | Acentos (`á ñ ¿`) | Símbolos FontAwesome (`⌫ ⏎ ✓`) |
|---|---|---|
| `font_es_*` (del proyecto) | **sí** | no |
| `lv_font_montserrat_16` (LVGL) | **no** | sí |

Comprobado en los datos: `font_es_20` tiene dos *cmaps* (`32–126` y `160–255`), y
el `unicode_list` de `lv_font_montserrat_16` son solo 62 entradas —ASCII, `°`, `•`
y el área `U+F000+`—, sin ningún acento.

El teclado estándar de LVGL rotula sus teclas de control con símbolos FontAwesome,
así que **ninguna de las dos fuentes sirve para un teclado español completo**.

### 3.2 Solución: teclado propio con `lv_buttonmatrix`

Etiquetas de control en español (que sí caben en las fuentes del proyecto):

```
 q   w   e   r   t   y   u   i   o   p
  a   s   d   f   g   h   j   k   l   ñ
MAYÚS  z   x   c   v   b   n   m   BORRAR
 á   é   í   ó   ú   ü   ¿   ¡  ESPACIO  INTRO
 0   1   2   3   4   5   6   7   8   9
```

- **Acentos incorporados**, así que se eliminó la barra de acentos que había
  encima: su espacio se aprovechó para teclas un 40 % más altas.
- `MAYÚS` cambia a mayúsculas (incluidas `Á É Í Ó Ú Ü Ñ`) y se **resalta en cian**
  mientras está activa.
- `INTRO` añade salto en la descripción; el título lo ignora. Verificado en el
  código de LVGL: `lv_textarea_add_text` va carácter a carácter cuando hay
  `max_length` (y ambas lo tienen), así que pasa por el filtro de `one_line`.
- El campo activo se marca con **borde cian**, para saber dónde escribe el teclado.
- `MICRÓFONO` se movió a la fila inferior, junto a `GUARDAR MISIÓN`.

---

## 4. Rotación automática

### 4.1 Causa

La pareja de ejes estaba **intercambiada**. El panel es nativo vertical
(720×1280), así que el eje **Y** del sensor recorre su lado largo y el **X** el
corto. En vertical la gravedad cae sobre el lado largo (Y); en horizontal, sobre
el corto (X). El código hacía lo contrario: con X dominante elegía vertical.

Se dedujo del remapeo de ejes del demo oficial de M5Stack
(`accelX = +raw.y`, `accelY = −raw.x`) y después se **verificó empíricamente** con
los valores que capturó el monitor mientras se movía el dispositivo:

| Valores crudos | Eje dominante | Postura |
|---|---|---|
| `x=195 y=15888` | Y (±16 000) | vertical |
| `x=-373 y=-16064` | Y (±16 000) | vertical invertida |
| `x=-15203 y=-1487` | X (±16 000) | horizontal |
| `x=16116 y=-843` | X (±16 000) | horizontal invertida |

### 4.2 Ajuste final

Con eso confirmado, el usuario reportó que las **verticales estaban bien** pero
las **horizontales salían de cabeza**, así que se invirtió únicamente esa pareja:

```cpp
if (std::abs(y) > std::abs(x) && std::abs(y) > threshold) {
    candidate = y > 0 ? 0 : 2;      // vertical
} else if (std::abs(x) > threshold) {
    candidate = x < 0 ? 3 : 1;      // horizontal
}
```

Se dejó un `ESP_LOGI` permanente que registra la rotación aplicada junto con los
valores crudos, que es lo que permitió este diagnóstico y servirá para cualquier
ajuste futuro.

---

## 5. Botón físico y apagado de pantalla

### 5.1 Hallazgo de hardware

El botón que se ve en el Tab5 (a la derecha del puerto USB‑A) **no es legible por
software**:

- La documentación de NuttX para el Tab5 lista **un solo botón**: BOOT en GPIO35.
  `PB_IN`/`PB_OUT` (GPIO16/17/52) son del bus de **alimentación**, no botones.
- El demo oficial de M5Stack **no lee ningún botón**; para apagar solo llama a
  `bsp_generate_poweroff_signal()`, que conmuta el bit 4 del expansor PI4IOE #2.
- La pulsación de ese botón **no generó ningún evento** de GPIO35, pero sí provocó
  5 reinicios de la aplicación: es un **latch de hardware** (o un reset).
- Se añadió una **sonda de solo lectura** sobre los registros de entrada de ambos
  expansores PI4IOE (`0x43` y `0x44`, registro `0x0F`): **no registró ni un solo
  evento**. La sonda se retiró después.

### 5.2 Qué se implementó

Sobre **GPIO35**, el único botón legible:

- **Pulsación corta** → apaga/enciende la pantalla. Con la pantalla apagada se
  dibuja una capa negra que **absorbe el tacto**, para que un roce no pulse
  botones invisibles. El **cronómetro sigue corriendo y guardando checkpoints**:
  apagar la pantalla no detiene una misión.
- **Pulsación mantenida (2 s)** → apagado ordenado: guarda el estado, apaga la
  retroiluminación y pulsa el latch. Si el equipo no se apaga (alimentado por USB,
  donde el latch solo corta la batería) lo explica en pantalla en lugar de quedarse
  en negro.
- Detección por tarea propia con antirrebote (25 ms) y cola de eventos, porque el
  tick de la UI (250 ms) es demasiado grueso para una pulsación.

### 5.3 Pendiente

Elegir la alternativa para apagar la pantalla desde el botón que el usuario sí
tiene: **botón en pantalla**, **doble toque** o **apagado por inactividad**.

---

## 6. Corrección del dashboard horizontal

### 6.1 Síntoma

En horizontal, la única información visible era el panel de **misión activa**: la
columna derecha (Zona de Enfoque y Backlog) quedaba completamente en blanco.

### 6.2 Causa raíz

En LVGL 9 los *getters* de geometría devuelven `obj->coords`, que **solo se
rellena en la pasada de layout**:

```c
int32_t lv_obj_get_width(const lv_obj_t * obj) { return lv_area_get_width(&obj->coords); }
```

Justo después de crear un objeto valen **0**, aunque se le haya fijado el tamaño.
El dashboard horizontal los usaba de inmediato, y un diagnóstico temporal lo dejó
en evidencia:

```
DIAG dashboard 1280x720 portrait=0 misiones=6 backlog=3 foco=3 activa=0 almacen=0%
DIAG paneles activo=(0,0,0,0) foco=(0,0,0,0) backlog=(0,0,0,0)
```

Los datos estaban bien (6 misiones, 3 en backlog, 3 en enfoque) pero los paneles
reportaban 0×0, y de ahí salían tres consecuencias:

1. Las filas de las listas se creaban con ancho `0 - 36 = **-36**` → **invisibles**.
2. El panel de backlog se colocaba en y=116 en vez de y=410 → **encima** del de
   enfoque.
3. Como sí había misiones, tampoco aparecía el mensaje de lista vacía.

Era un defecto **preexistente** que solo podía verse cuando la rotación empezó a
funcionar: con la rotación invertida, sostener el Tab5 en horizontal mostraba la
composición vertical.

### 6.3 Corrección

- El dashboard calcula su geometría con **variables locales**.
- El mismo fallo estaba en la pantalla de reloj recién escrita
  (`rollers_bottom = kRollerY + lv_obj_get_height(...)`), que dibujaba la fila de
  zona horaria y el botón de guardar **encima de las ruedas**. Ahora las ruedas
  fijan su altura explícitamente con `clock_roller_height()`.
- Se añadió una nota en el README para no repetirlo.

---

## 7. Documentación actualizada

- `README.md` (raíz): estado v0.2.0.
- `firmware/README.md`: sección **Pruebas de dominio**, nota del intérprete de
  Python, mención de las ruedas del reloj y sección **Trampa de LVGL a evitar**.
- `firmware/main/fonts/README.md`: aviso de que las fuentes no incluyen el área de
  símbolos `U+F000+` y por qué el teclado no las usa.

---

## Hallazgos del entorno (importantes para retomar)

1. **Hay dos instalaciones de ESP‑IDF** y el build pertenece a la 5.4.2:

   | Ruta | Versión |
   |---|---|
   | `~/.espressif/v5.4.2/esp-idf` | **5.4.2** ← la que usa el proyecto |
   | `~/esp/esp-idf` | 5.4.4 |

   El directorio `build/` está configurado con el intérprete de la extensión de
   VS Code (`~/.espressif/tools/python/v5.4.2/venv/bin/python`). Si se invoca
   `idf.py` con otro intérprete, aborta con un `FatalError` que sugiere
   `fullclean`; **no hace falta**: basta usar ese mismo Python.

2. **La consola primaria es UART0** y el USB‑Serial‑JTAG es secundaria. Todo lo que
   use `printf`/UART del ROM (Unity, mensajes del bootloader) **no llega por USB**.

3. **Ninguna fuente cubre acentos y símbolos a la vez.** Si en el futuro se quiere
   usar el teclado estándar de LVGL, habría que regenerar las fuentes con
   `lv_font_conv` añadiendo el rango de símbolos.

4. **El botón de encendido no es legible por software.** Cualquier función que
   dependa de él debe resolverse con la pantalla táctil.

5. `firmware/dependencies.lock` está en `.gitignore`, así que no se versiona el
   pin de los componentes gestionados (`bmi270_sensor`, `i2c_bus`).

---

## Archivos modificados

**Nuevos**

- `firmware/main/{model,services,repository,hardware,ui}.{cpp,hpp}`
- `firmware/main/idf_component.yml`
- `firmware/main/test/{CMakeLists.txt,test_domain.cpp}`

**Modificados**

- `firmware/main/app_main.cpp` — arranque de pruebas, servicio de botón, composición
- `firmware/main/{ui,hardware}.{cpp,hpp}` — ruedas, teclado, rotación, botón
- `firmware/main/CMakeLists.txt` — `PRIV_REQUIRES unity` y bandera de pruebas
- `firmware/main/test/CMakeLists.txt` — `WHOLE_ARCHIVE`
- `firmware/CMakeLists.txt` — opción `EL_SISTEMA_TESTS`
- `firmware/sdkconfig.defaults` — `CONFIG_UNITY_ENABLE_64BIT`
- `firmware/README.md`, `firmware/main/fonts/README.md`, `README.md`

---

## Estado de verificación

| Comprobación | Resultado |
|---|---|
| Compilación sin advertencias propias | Sí |
| Arranque en el Tab5 sin *panic* ni aserciones | Sí |
| Pruebas de dominio en el dispositivo | 4 ejecutadas, 0 fallidas |
| Teclado español | Verificado por el usuario |
| Ruedas de fecha y hora | Verificado por el usuario |
| Rotación vertical | Verificado por el usuario |
| Rotación horizontal | Corregida y flasheada; **falta confirmación visual** |
| Dashboard horizontal | Corregido y flasheado; **falta confirmación visual** |

---

## Pendientes

1. Confirmar las dos posturas horizontales tras el último ajuste.
2. Elegir alternativa para apagar la pantalla (botón en pantalla, doble toque o
   inactividad), ya que el botón físico quedó descartado.
3. El editor está maquetado **solo para horizontal**: la fila de RANGO y
   RECORDATORIO llega hasta x=906, así que en vertical se sale de pantalla.
4. Rediseñar la pantalla del reloj no se puede verificar hasta que el RTC vuelva a
   quedar sin hora válida (solo aparece en ese caso).

---

## Cómo compilar, probar y flashear

```bash
source ~/.espressif/v5.4.2/esp-idf/export.sh
cd firmware

# Compilación normal
~/.espressif/tools/python/v5.4.2/venv/bin/python "$IDF_PATH/tools/idf.py" build

# Con las pruebas de dominio (se ejecutan al arrancar)
~/.espressif/tools/python/v5.4.2/venv/bin/python "$IDF_PATH/tools/idf.py" \
    -DEL_SISTEMA_TESTS=ON -p /dev/tty.usbmodem2101 flash monitor
```

Al flashear **no se borra SPIFFS**: `idf.py flash` escribe bootloader, tabla de
particiones y aplicación, pero no toca la partición `storage`, donde vive el estado
de las misiones.
