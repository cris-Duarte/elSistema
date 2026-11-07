# Tab5 Voxel 0.1.1

Firmware independiente para M5Stack Tab5 (ESP32-P4). Incluye mundo procedural
64×64×32 en PSRAM, cámara en primera persona, raycasting DDA, colisiones, gravedad,
salto, seis bloques seleccionables y edición del terreno. Arranca con menú.

**Estado:** código implementado y compilado con ESP-IDF 5.4.2. Núcleo probado en
host con ASan/UBSan. Arranque, pantalla, tacto, FPS y estabilidad en el Tab5 están
pendientes de prueba física. El puerto `/dev/cu.usbmodem101` aparece, pero el
entorno del agente rechaza abrirlo con `Operation not permitted`; no se pudo
leer la identidad ni flashear el dispositivo. La configuración
PlatformIO/Arduino se entrega, pero no pudo verificarse en este entorno sin acceso
al servidor de paquetes. Consulta [el estado de validación](../docs/voxel/estado.md).

## Jugar

- `JUGAR` o `MENU`: entrar/salir de la pausa.
- Joystick izquierdo: avanzar, retroceder y desplazarse lateralmente.
- Arrastrar en la derecha, fuera de botones: mirar.
- `SALTO`: saltar si hay suelo.
- `MINA` y `PONER`: una acción por toque, alcance máximo de cinco bloques.
- Slots 1–6: tierra, césped, piedra, madera, hojas y arena.
- `TEST`: alternar patrón de pantalla y diagnóstico de memoria/contactos.

Se pueden combinar movimiento, mirada y acción. Un contacto conserva su función
hasta levantarse. Al cambiar de menú se cancelan los contactos que siguen
apoyados para evitar acciones accidentales. La capa inferior no puede romperse.
No hay guardado: el mundo original vuelve a generarse al reiniciar.

En 0.1.1 se corrigió la pérdida de rayos al entrar por los bordes del mapa por
redondeo de coma flotante. Las seis caras y las entradas en esquinas tienen
pruebas de regresión. Se añadió un perfil de benchmark autónomo.

## Compilar con PlatformIO / Arduino

Requiere PlatformIO Core y acceso a sus paquetes. Desde esta carpeta:

```bash
pio run -e tab5
pio run -e tab5_diagnostic
```

El perfil `tab5_quality` usa 256×144 y es experimental hasta medir rendimiento.
El perfil inicial usa RGB565 a 160×90, con escalado entero a 1280×720. La
plataforma pioarduino, M5Unified y M5GFX están fijadas en `platformio.ini`.

`tools/pio_m5gfx.py` añade un getter al `Panel_DSI` de la copia descargada de
M5GFX. El parche verifica la estructura esperada y los dos buffers. No modifica
los drivers del firmware existente. Un cambio incompatible de biblioteca provoca
un error de build explícito en lugar de recurrir a una ruta visual sin sincronizar.

PlatformIO debe activar la PSRAM del P4. `BOARD_HAS_PSRAM` no sustituye la
configuración de la placa y su SDK. Confirmar en el log de arranque 32 MB de
PSRAM y su frecuencia antes de evaluar FPS. La combinación Arduino queda pendiente
de compilación real, incluidos el orden del script y la configuración de PSRAM.

## Compilar con ESP-IDF 5.4.2

El mismo código tiene un punto de entrada nativo `app_main`, además de `setup/loop`
para Arduino. No incorpora LVGL, Wi-Fi, cámara ni los módulos del firmware anterior.
Después de activar ESP-IDF 5.4.2:

```bash
python3 tools/prepare_idf.py
IDF_COMPONENT_MANAGER=0 idf.py -B build-idf build
```

El preparador descarga los tags de M5GFX 0.2.32 y M5Unified 0.2.25, aplica el getter
y registra SHA-256 de los árboles preparados en `components/sources.json`.
Los componentes preparados y los builds se excluyen de git; pueden regenerarse.

Para reproducir la compilación sin red con la caché disponible en este repositorio:

```bash
python3 tools/prepare_idf.py --from-existing ../firmware/managed_components
IDF_COMPONENT_MANAGER=0 idf.py -B build-idf build
```

La copia offline es independiente una vez preparada. Su procedencia y hash quedan
registrados: no se presenta como una verificación de identidad con los tags remotos.
Para builds separados:

```bash
IDF_COMPONENT_MANAGER=0 idf.py -B build-diagnostic -DVOXEL_DIAGNOSTIC=ON build
IDF_COMPONENT_MANAGER=0 idf.py -B build-quality -DVOXEL_QUALITY=ON build
```

Cada directorio tiene su propio `sdkconfig`. El SDK 5.4.2 exige habilitar
`CONFIG_IDF_EXPERIMENTAL_FEATURES` para seleccionar PSRAM a 200 MHz; los defaults
lo incluyen. Si el arranque no es estable, diagnosticar alimentación, revisión y
memoria antes de cambiar la frecuencia. No asumir una frecuencia por el nombre
del perfil: revisar `build-*/sdkconfig` y el log real.

La salida real contiene `tab5_voxel.bin`, `tab5_voxel.elf`, bootloader, tabla de
particiones y `flasher_args.json`. `tab5_voxel.bin` es solo la aplicación, no una
imagen fusionada para escribir desde dirección cero.

Para empaquetar un build real, incluyendo offsets y checksums:

```bash
python3 tools/package_release.py --build build-idf --output build-idf/tab5-voxel.zip
```

Cuando se decida instalar, identificar primero el puerto. Desde esta carpeta,
conservar las particiones y offsets generados por el build seleccionado:

```bash
IDF_COMPONENT_MANAGER=0 idf.py -B build-idf -p PUERTO flash monitor
```

Ese comando instala este firmware independiente y su tabla de particiones en el
Tab5. Para volver al firmware anterior se utiliza su build/backup. No ejecutar
`erase-flash` para esta prueba. No se incluyen credenciales ni se programa el C6.

## Pruebas en el ordenador

Requiere CMake y Clang/GCC; no requiere ESP-IDF ni drivers:

```bash
cmake -S test -B build-host -DVOXEL_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure
./build-host/voxel_tests build-host/preview.ppm
```

Para la otra resolución, configurar otro directorio con `-DVOXEL_QUALITY=ON`.
Las pruebas cubren fallos de reserva, límites, determinismo del terreno, DDA contra
un oráculo independiente de intersecciones, bordes/empates, AABB, salto/techo,
normalización diagonal, alcance/colocación, paso fijo, contactos reordenados,
cancelación en pausa y escalado en las ocho orientaciones. También comprueban
canarios de la imagen y ausencia de reservas de la arena durante el render/HUD.

`preview.ppm` contiene una imagen renderizada por el núcleo real en host. No es
una captura del Tab5 ni evidencia de sus FPS.

## Prueba automática de rendimiento

El perfil de benchmark no necesita interacción táctil. Recorre `vacio`, `plano`,
`bosque` y `construccion`, con tres segundos de calentamiento y un minuto de
medición por escena. Ignora los controles y no modifica el mundo durante la
medición. Al finalizar sigue mostrando la última escena hasta reiniciar.

```bash
IDF_COMPONENT_MANAGER=0 idf.py -B build-benchmark -DVOXEL_BENCHMARK=ON build
```

También existe `pio run -e tab5_benchmark`; esa ruta sigue sin verificación de
build en este entorno. El binario ESP-IDF de benchmark sí se ha compilado.

Para una prueba física, verificar primero la identidad del Tab5 y el respaldo
como indica [backup-tab5.md](../docs/backup-tab5.md). Cuando el puerto sea
accesible, instalar el build elegido desde una terminal con ESP-IDF activo:

```bash
IDF_COMPONENT_MANAGER=0 idf.py -B build-benchmark -p /dev/cu.usbmodem101 flash
python tools/capture_benchmark.py --port /dev/cu.usbmodem101 --output build-benchmark/hardware.log
```

El puerto puede cambiar. Tras abrir la captura, reiniciar manualmente el Tab5
para registrar desde el inicio. El capturador usa pyserial del entorno ESP-IDF,
no envía comandos ni flashea y rechaza sobrescribir un log existente. Cierra
cualquier otro monitor que use el mismo puerto.

Después de unos cuatro minutos y doce segundos, analizar el archivo:

```bash
python3 tools/benchmark_report.py build-benchmark/hardware.log --output build-benchmark/hardware.json
```

La salida incluye FPS globales, mínimo en ventanas completas de al menos cinco
segundos, tiempos medios de render/presentación y memoria libre mínima. Devuelve
código 0 solo si están las cuatro escenas terminadas, sin errores de dispositivo
detectados, y todas cumplen 20 FPS globales y por ventana. Un log parcial o datos
incoherentes no se convierten en un resultado aprobado. Un nuevo arranque descarta
la ejecución anterior, incluso si aquella había terminado. Se admiten los
colores ANSI de la consola del SDK.

El benchmark no comprueba tacto, tearing visible ni una sesión de juego de
treinta minutos. Sus métricas se basan en las confirmaciones de frames que
observa la tarea, con el mismo alcance explicado en el apartado de diagnóstico.
Las pruebas del agregador/parser usan relojes y datos sintéticos etiquetados;
no son mediciones del Tab5.

## Arquitectura y memoria

- `World` y sus revisiones de chunks usan una reserva PSRAM: 131 072 bytes de
  voxels y 64 bytes de metadatos. No hay mallas ni remallado.
- `Renderer` reserva una tabla de longitudes de rayos en PSRAM al inicio;
  evita raíces/trigonometría por píxel y reutiliza paletas de caras.
- Dos imágenes internas RGB565 y dos framebuffers nativos del driver. El escalado
  nearest-neighbor recorre filas físicas contiguas; la misma transformación de
  orientación se comprueba en host.
- El backend usa `esp_lcd_panel_draw_bitmap` con el puntero de un framebuffer
  nativo: el driver hace el writeback de caché y selecciona el siguiente buffer.
  Dos callbacks de refresh posteriores cubren conservadoramente la carrera de
  envío antes de reutilizar el buffer anterior. El siguiente render puede avanzar
  mientras se refresca el panel. Un timeout detiene la presentación y conserva
  la última imagen; el error se comunica por consola.
- Después del primer envío no se dibuja mediante `M5.Display`: sus punteros de
  filas seguirían apuntando a un buffer que podría estar escaneándose.
- Una sola tarea modifica mundo, cámara, física e imagen. La ISR únicamente
  incrementa un contador atómico lock-free. No hay reservas del juego por frame.

El doble buffer y la sincronización están implementados conforme al driver
ESP-IDF 5.4.2 revisado. Eso no sustituye la prueba de tearing/underrun en hardware.
No cambiar de SDK mayor sin revisar las garantías del driver DSI.

## Diagnóstico físico pendiente

Primero ejecutar `TEST` o el build de diagnóstico. Confirmar rojo/verde/azul,
arriba/abajo, movimiento de la línea, orientación táctil y tres contactos
simultáneos. Observar al menos diez minutos buscando tearing, underrun y resets.

La consola imprime cada cinco segundos FPS de imágenes nuevas confirmadas,
duraciones de render/presentación, pasos medios DDA y memoria. El p95 mostrado
es de intervalos **observados por la tarea**, no de timestamps de scanout
instrumentados en ISR; sirve para diagnóstico inicial y no certifica por sí
solo latencia táctil ni cadencia del panel. Las duraciones no incluyen todos los
costes del bucle y `present` puede incluir espera del envío anterior.

Para aceptar rendimiento: medir 60 s por escena en cielo, terreno plano, bosque
y estructura editada, registrando semilla, vista, resolución, distancia, versión
y alimentación. Mínimo objetivo: 20 FPS; preferido: 30. Para aceptar estabilidad:
30 min caminando, saltando y editando. Aún no hay resultados físicos.
