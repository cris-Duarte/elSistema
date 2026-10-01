# El Sistema v0.2.0 — firmware para M5Stack Tab5

MVP local del módulo de productividad, construido con ESP-IDF 5.4.2, el BSP
oficial de M5Stack Tab5 y LVGL 9.2.2. No requiere Wi‑Fi ni servicios externos.

## Funciones incluidas

- Creación y edición de misiones con teclado táctil español, descripción,
  rango E–S y recordatorio configurable.
- Zona de Enfoque ordenable, un único cronómetro activo e historial de
  intervalos.
- Pausa automática al cambiar de misión, recordatorios visuales en cola y
  acciones para retomar, posponer o cancelar.
- XP por rango y tiempo activo, niveles progresivos y recompensa animada.
- Historial persistente con búsqueda sin distinguir mayúsculas ni tildes,
  filtros por rango/fecha, orden y detalle de intervalos.
- Persistencia JSON versionada en SPIFFS con archivo temporal y respaldo; NVS
  para preferencias y checkpoints cada 30 segundos.
- Recuperación segura del cronómetro como pausado después de reiniciar.
- RTC RX8130CE con configuración inicial por ruedas deslizables (día, mes, año,
  hora y minuto, sin teclado), y rotación automática con BMI270 después de
  800 ms de orientación estable.
- Composiciones horizontal y vertical. La rotación se bloquea durante el
  teclado y los diálogos.

El botón de micrófono muestra `Dictado no configurado`. Audio local está
reservado para v0.3.0 y el dictado privado mediante Whisper para v0.4.0.

## Arquitectura

- `model.*`: estado de dominio y tipos persistentes.
- `services.*`: misiones, Zona de Enfoque, cronómetro, recordatorios y XP.
- `repository.*`: JSON/SPIFFS, respaldo atómico, migración y NVS.
- `hardware.*`: RTC y sensor de orientación.
- `ui.*`: pantallas LVGL y traducción de acciones táctiles a servicios.
- `app_main.cpp`: composición e inicio del dispositivo.

Los datos principales se guardan en `/spiffs/el_sistema.json`; se usan
`el_sistema.tmp` y `el_sistema.bak` durante las escrituras y recuperaciones.

### Trampa de LVGL a evitar

`lv_obj_get_width()`, `lv_obj_get_height()`, `lv_obj_get_x()` y `lv_obj_get_y()`
devuelven `obj->coords`, que LVGL solo rellena en la pasada de *layout*. Justo
después de crear un objeto valen **0**, aunque se le haya fijado el tamaño con
`lv_obj_set_size()`. Usarlos en ese momento produce filas con ancho negativo
(invisibles) o paneles superpuestos. En `ui.cpp` la geometría siempre se calcula
con variables locales, y cuando hace falta una altura conocida (las ruedas del
reloj) se fija explícitamente.

## Compilar

Activa ESP-IDF 5.4.2 y ejecuta:

```bash
cd firmware
idf.py build
```

El directorio `build/` está configurado por la extensión ESP-IDF de VS Code, que
usa su propio intérprete (`~/.espressif/tools/python/v5.4.2/venv/bin/python`). Si
`idf.py` desde la terminal avisa de un desajuste de intérprete, invócalo con ese
mismo Python en lugar de hacer `fullclean`:

```bash
~/.espressif/tools/python/v5.4.2/venv/bin/python "$IDF_PATH/tools/idf.py" build
```

## Pruebas de dominio

Las pruebas viven en `main/test/test_domain.cpp` (componente `test`) y cubren XP,
niveles, búsqueda con acentos y localización de misiones. **No** forman parte del
build normal: se activan con una opción de CMake y el firmware las ejecuta al
arrancar, antes de tocar hardware o almacenamiento, en una tarea propia.

```bash
cd firmware
idf.py -DEL_SISTEMA_TESTS=ON build
idf.py -p /dev/tty.usbmodem2101 -DEL_SISTEMA_TESTS=ON flash monitor
```

El resultado se informa por el log de IDF, prueba por prueba:

```
I (1125) el_sistema: Pruebas de dominio registradas: 4
I (1155) el_sistema:   XP aplica bases, minutos completos y límite temporal      OK
I (1174) el_sistema:   una recompensa puede subir varios niveles                  OK
I (1198) el_sistema:   la búsqueda ignora mayúsculas y acentos españoles       OK
I (1219) el_sistema:   el modelo localiza misiones por identificador estable      OK
I (1220) el_sistema: Resultado de pruebas: 4 ejecutadas, 0 fallidas
```

Con `-DEL_SISTEMA_TESTS=OFF` (valor por defecto) el componente `test` no entra en
el build y el binario no cambia de tamaño.

Tres detalles del entorno que conviene no redescubrir:

- La consola primaria es UART0 y la salida detallada de Unity usa la UART del ROM,
  que en el Tab5 no llega por USB. Por eso el resumen se emite con `ESP_LOGI`; si
  conectas UART0 verás además el detalle de Unity.
- `CONFIG_UNITY_ENABLE_64BIT=y` es necesario: sin él, `TEST_ASSERT_EQUAL_UINT64`
  falla siempre con `UnityStrErr64`, porque `Profile::total_xp` es `uint64_t`.
- El componente `test` se registra con `WHOLE_ARCHIVE`; sin eso el enlazador no
  extrae el objeto de la librería estática (nada lo referencia) y el runner
  informaría cero pruebas.

## Flashear y observar el arranque

```bash
idf.py -p /dev/tty.usbmodem2101 flash monitor
```

El puerto puede cambiar. Para salir del monitor usa `Ctrl + ]`.

## Validación en el Tab5

1. Configura el RTC, completa el tutorial y reinicia para confirmar que no se
   repiten.
2. Crea y edita misiones usando `á é í ó ú ü ñ ¿ ¡`.
3. Alterna varias misiones y comprueba que solo una suma tiempo.
4. Reinicia durante una misión; debe reaparecer pausada y conservar el último
   checkpoint (pérdida máxima aproximada: 30 segundos).
5. Verifica recordatorios, XP, historial, búsqueda y ambas orientaciones.

No borres ni reformatees SPIFFS durante estas pruebas: allí vive el estado del
MVP.
