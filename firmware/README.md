# El Sistema v0.4.2 — firmware para M5Stack Tab5

MVP local del módulo de productividad, construido con ESP-IDF 5.4.2, el BSP
oficial de M5Stack Tab5 y LVGL 9.2.2. El dictado requiere además
Wi-Fi local y el servidor del MacBook (`tools/whisper/`); sin red la app
sigue funcionando con teclado.

Los módulos disponibles son Productividad, Asistente, Tetris y Hábitos.

## Funciones incluidas

- Módulo **HÁBITOS** (ID 6): diarios con metas semanales flexibles y mosaico de
  doce semanas; continuos con contador RTC e historial de periodos. JSON propio
  en SPIFFS, avisos visuales/tono/voz y estética clara. Sin MIC ni XP.
  [Documentación](../docs/habitos/README.md) y [diseño](../docs/habitos/design.md).

- Módulo **TETRIS**: tablero retro 12×20 con cuadrados de 60 px, gestos, próxima pieza, niveles,
  puntuación/récord, partida recuperable en NVS, efectos y música originales.
  Vertical fija, pausa ante recordatorios y audio de baja prioridad.
  Las partidas anteriores conservan sus 10 columnas al continuar.
  Documentación y pruebas en [docs/tetris/README.md](../docs/tetris/README.md).

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
- Barra superior de estado (oculta en módulos inmersivos): reloj local, control táctil de
  conectividad de 44 px con estados `BUSCANDO RED`, `CONECTANDO`, SSID,
  `BUSCANDO MAC`, `LISTO` o `SIN CONEXIÓN`, y energía (`USB · BAT %`
  cargando, `BAT %` con batería, `USB` solo sin batería instalada). El
  porcentaje usa la fórmula de M5Unified (por celda 3300–4100 mV, lineal
  aproximada) sobre el monitor INA226.
- Composiciones horizontal y vertical. La rotación se bloquea durante el
  teclado y los diálogos.
- Módulo experimental `ASISTENTE · A2` con personaje original, rostro dibujado
  por LVGL y máquina de estados no bloqueante. `HABLAR` realiza un turno real:
  captura por micrófono, transcripción local con Whisper y confirmación hablada
  integrada. La respuesta aún es determinista; no hay IA conversacional,
  memoria ni funciones sociales.

El botón MIC abre el dictado: graba (hasta 30 s, parada por segundo toque o
3 s de silencio después de detectar voz), muestra nivel y RMS por slot TDM, envía el WAV a
`POST http://<mac-descubierto>:8080/inference` y abre el editor con la transcripción como
título editable. Aun sin configurar Wi-Fi permite hacer una captura local,
identifica los dos slots de micrófono y el de referencia, y reproduce la muestra
por el altavoz para diagnosticar el audio. El teclado sigue disponible siempre.
Detalle del servidor en `tools/whisper/README.md`.

## Arquitectura

- `model.*`: estado de dominio y tipos persistentes.
- `services.*`: misiones, Zona de Enfoque, cronómetro, recordatorios y XP.
- `repository.*`: JSON/SPIFFS, respaldo atómico, migración y NVS.
- `hardware.*`: RTC y sensor de orientación.
- `audio.*`: ES7210/ES8388, diagnóstico TDM, FIR 48→16 kHz, PCM y WAV en PSRAM.
- `audio_session.*`: propietario único del codec para dictado, asistente y avisos.
- `assistant.*`: máquina de estados visual del personaje.
- `assistant_voice.*`: coordinador A2 de captura, transcripción y respuesta
  local; no llama a LVGL desde su worker.
- `assets/assistant_character.*`: personaje original optimizado para LVGL; las
  expresiones se superponen en tiempo real para evitar almacenar varios sprites.
- `ble.*`: prueba A0 de NimBLE sobre ESP-Hosted/VHCI, sin credenciales ni acciones.
- `connectivity.*`: ESP-Hosted/SDIO, hasta ocho perfiles Wi-Fi en NVS,
  reconexión priorizada y descubrimiento Bonjour/mDNS del Mac.
- `transcription.*` y `dictation.*`: cliente Whisper y máquina de estados sin
  llamadas a LVGL desde tareas de captura o red.
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
niveles, búsqueda con acentos, límites UTF-8, remuestreo 48→16 kHz, WAV, análisis
de Whisper, serialización de perfiles, prioridad de redes y selección de Mac.
**No** forman parte del build normal: se activan con
una opción de CMake y el firmware las ejecuta al arrancar, antes de tocar hardware
o almacenamiento, en una tarea propia.

```bash
cd firmware
idf.py -DEL_SISTEMA_TESTS=ON build
idf.py -p /dev/tty.usbmodem2101 -DEL_SISTEMA_TESTS=ON flash monitor
```

El resultado se informa por el log de IDF, prueba por prueba:

```
I (1125) el_sistema: Pruebas de dominio registradas: 20
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
6. Comprueba la barra superior: el reloj avanza cada minuto, el control Wi-Fi
   abre `CONECTIVIDAD` y al enchufar/desenchufar el USB-C aparece `USB`.
7. Sin configurar Wi-Fi, toca MIC, dicta una frase y detén la captura. Debe
   aparecer `Muestra local`, el mapa `MIC Sx/Sy · REF Sz` y `REPRODUCIR` debe
   emitir la muestra sin congelar la interfaz.
8. Guarda al menos dos redes y comprueba prioridad, cambio automático y olvido.
   Ejecuta `tools/whisper/servidor.sh`: el Mac debe aparecer automáticamente en
   `CONECTIVIDAD → MACBOOK`, sin introducir IP ni puerto. Verifica grabación,
   transcripción, edición y descarte. La misión solo debe existir después de
   pulsar `Guardar`.
9. Apaga el servidor y repite: el audio debe permanecer únicamente en PSRAM y
   ofrecer `Reintentar` o `Descartar` sin afectar el cronómetro activo.
10. En `CONECTIVIDAD → ESTADO` comprueba `BLUETOOTH · PRUEBA A0`. Desde una
    aplicación BLE genérica busca `El Sistema`, conecta y escribe `ping` en la
    única característica. El contador `PINGS` debe aumentar sin perder Wi-Fi.
    El servicio de diagnóstico es `01000000-6c65-2d73-6973-74616d657a10` y su
    característica de lectura/escritura es
    `02000000-6c65-2d73-6973-74616d657a10`. Al leerla devuelve estado, contador
    y RAM interna libre/mínima en JSON.
11. Con `tools/whisper/servidor.sh` activo, abre `ASISTENTE` y toca `HABLAR` o
    el personaje. Di una frase y detén la captura: debe recorrer `ESCUCHANDO`,
    `TRANSCRIBIENDO` y `RESPONDIENDO`, mostrar lo entendido y reproducir
    “Te escucho. El canal de voz está listo.” sin congelar la interfaz. Repite
    con una misión activa y confirma que su cronómetro continúa. `CICLO VISUAL`
    y `CELEBRAR` siguen disponibles como diagnósticos locales.

No borres ni reformatees SPIFFS durante estas pruebas: allí vive el estado del
MVP.
