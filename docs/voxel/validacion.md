# Validación de Tab5 Voxel 0.1.0

Fecha: 7 de noviembre de 2026.

## Comprobaciones realizadas

| Comprobación | Resultado |
| --- | --- |
| Núcleo host a 160×90 | ASan + UBSan, 137 041 comprobaciones, 0 fallos |
| Núcleo host a 256×144 | ASan + UBSan, 137 041 comprobaciones, 0 fallos |
| Imagen del núcleo en host | PPM generado; inspección visual del terreno, árboles, retícula, slots y botones |
| Build ESP32-P4 normal | ESP-IDF 5.4.2, correcto; aplicación, bootloader y particiones generados |
| Build ESP32-P4 diagnóstico | ESP-IDF 5.4.2, correcto; arranque de patrón activado por CMake |
| Configuración generada de ambos builds | P4 a 360 MHz, flash 16 MB, PSRAM seleccionada a 200 MHz |
| Paquetes ZIP | Integridad ZIP y SHA-256 de cada archivo registrados |
| Python de preparación/parche/empaquetado | Sintaxis comprobada; preparación offline, parche y empaquetado ejecutados |
| PlatformIO/Arduino | No compilado: PlatformIO ausente y descarga bloqueada por resolución de `pypi.org` |
| Arranque y controles en Tab5 | No probado |
| FPS, tearing, underrun, consumo y estabilidad | No medidos en hardware |

Las 137 041 comprobaciones son aserciones dentro de una suite de dominio, no
137 041 escenarios independientes. Incluyen recorrer los límites/materiales del
mundo y contrastar 1 500 rayos con un oráculo de intersección de cajas.
Las duraciones del test en el ordenador no se usan para inferir FPS del Tab5.

## Builds ESP32-P4

Preparación ejecutada desde `voxel_firmware/`:

```bash
python3 tools/prepare_idf.py --from-existing ../firmware/managed_components
```

Builds finales, tras activar ESP-IDF 5.4.2 y usando su entorno Python:

```bash
IDF_COMPONENT_MANAGER=0 idf.py -B build-idf -D "SDKCONFIG=$PWD/build-idf/sdkconfig" build
IDF_COMPONENT_MANAGER=0 idf.py -B build-diagnostic -DVOXEL_DIAGNOSTIC=ON build
```

Se corrigieron dos hallazgos antes de obtener los builds finales:

- El tipo `int32_t` del toolchain P4 se resuelve como `long`, a diferencia del
  host. Se convirtió explícitamente la altura antes de `std::clamp`.
- ESP-IDF 5.4.2 ignoraba la selección de PSRAM a 200 MHz sin habilitar las
  características experimentales. Se añadió la opción y se verificó el
  `sdkconfig` final. Esto verifica configuración, no funcionamiento físico.

Los avisos restantes proceden de cabeceras del SDK y de la compilación de las
bibliotecas gráficas; no hubo errores al generar los binarios finales.

| Aplicación | Tamaño | SHA-256 |
| --- | ---: | --- |
| Normal, `build-idf/tab5_voxel.bin` | 487 600 bytes | `9acbf1351c154ac963f427ee9de05f463ec2a262f2a970e370da70dfd6a93d0e` |
| Diagnóstico, `build-diagnostic/tab5_voxel.bin` | 487 600 bytes | `202eadb0d9eb93a66de62fd2d8a2a7ad489b274d1a8de8a0d8a2076dd262de13` |

La tabla generada indica bootloader en `0x2000`, particiones en `0x8000` y
aplicación en `0x10000`. El empaquetador toma esos offsets del build; no supone
que sean válidos para otro proyecto. El build usa flash DIO a 80 MHz según
`flasher_args.json`, aunque la opción lógica de arranque en Kconfig sea QIO.

Bibliotecas usadas en esta compilación: copias locales preparadas de M5GFX
0.2.32 y M5Unified 0.2.25. Hashes de los árboles después de aplicar el getter:

- M5GFX: `d20bec9d3d9cebd09654748da85b1685a7c7165bd1d7fe3536827135cd142bc4`.
- M5Unified: `1c3ca26173a9ff9c3426545c102b2555ce6556c49655c0c56835826d94d77703`.

Estos hashes identifican las copias efectivamente usadas. No certifican que las
copias preexistentes sean idénticas a una descarga nueva de los tags upstream.

## Prueba física para cerrar el MVP

1. Instalar primero el build de diagnóstico cuando se disponga del equipo y se
   haya elegido reemplazar el firmware instalado. Mantener su backup/build.
2. Confirmar detección del panel, 32 MB de PSRAM y su frecuencia efectiva; revisar
   colores, orientación, tres contactos y patrón durante diez minutos.
3. Ejecutar el juego normal y probar suelo, paredes, techo, salto, slots, alcance,
   colocar junto al jugador, bordes, pausa y pérdida de contactos.
4. Medir escenas reproducibles del plan durante 60 s cada una. Objetivo mínimo
   de 20 FPS; 30 FPS preferidos. El p95 del HUD es observación de la tarea y debe
   contrastarse si se necesita certificar cadencia/latencia.
5. Completar una sesión de 30 min y comprobar memoria, reset y watchdog.

El Tab5 no se ha flasheado ni borrado durante esta implementación. Las
condiciones anteriores permanecen abiertas, por lo que aún no se declara
validado el MVP en hardware.
