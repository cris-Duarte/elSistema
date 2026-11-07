# Validación de Tab5 Voxel 0.1.1

Fecha: 7 de noviembre de 2026. Continuación de la implementación inicial.

## Cambios y evidencia

- Se añadieron rayos que parten fuera del mundo. Antes de la corrección, una
  prueba de 3 000 entradas por la cara X negativa produjo 1 497 fallos.
  `origin + direction * entry` podía redondear a un valor negativo diminuto;
  `floor()` lo convertía en celda -1 y descartaba el rayo.
- El DDA reconstruye exactamente las coordenadas de las caras de entrada,
  incluidas las caras empatadas al entrar por una esquina. La regresión final
  prueba 18 000 entradas desde las seis caras y una esquina.
- El perfil autónomo `VOXEL_BENCHMARK` prepara cuatro mundos/vistas reproducibles
  y mide un minuto por escena, tras tres segundos de calentamiento. Emite logs
  JSON con frames confirmados, FPS, mínimo por ventanas de cinco segundos,
  tiempos medios de render/presentación y memoria mínima.
- Las herramientas de captura e informe conservan los logs, admiten colores
  ANSI y rechazan datos incoherentes o ejecuciones incompletas. Las pruebas del
  parser usan fixtures sintéticos identificados; no son resultados del Tab5.

## Resultado de verificaciones

| Verificación | Resultado |
| --- | --- |
| Núcleo 160×90 con ASan/UBSan | 155 089 comprobaciones, 0 fallos |
| Núcleo 256×144 con ASan/UBSan | 155 089 comprobaciones, 0 fallos |
| Parser de benchmark | 9 pruebas Python, correctas |
| Build normal ESP32-P4 | Correcto, ESP-IDF 5.4.2 |
| Build diagnóstico ESP32-P4 | Correcto, ESP-IDF 5.4.2 |
| Build benchmark ESP32-P4 | Correcto, ESP-IDF 5.4.2 |
| Macros/perfiles en comandos y binarios | Normal sin macros; diagnóstico y benchmark activados en sus builds |
| Paquetes | Incluyen versión/perfil del build, offsets generados y SHA-256 |

Los recuentos del núcleo son aserciones dentro de una suite, no escenarios
independientes. La comprobación del benchmark en host usa reloj simulado y no
pretende demostrar rendimiento en el P4.

| Aplicación | Tamaño | SHA-256 |
| --- | ---: | --- |
| Normal | 487 744 bytes | `38f1abe0f0ea7612294e5cc9baddb303678a6af228be314b970eaf8c809f6653` |
| Diagnóstico | 487 744 bytes | `e1a56c9794db3ea45cc8b04bd891f5f36ae8824929250a2e16cbbe5cabda4318` |
| Benchmark | 487 856 bytes | `c88b12f6357c7be91a3062832cac79ffe03547d57800cf6fc22c35441d031da7` |

## Bloqueos físicos y de paquetes

El sistema enumera `/dev/cu.usbmodem101` y `/dev/tty.usbmodem101`. Se intentó:

```bash
python -m esptool --chip esp32p4 --port /dev/cu.usbmodem101 --baud 460800 chip_id
```

La apertura devolvió `Operation not permitted`. El comando terminó antes de
comunicarse con el chip. No se pudo verificar la identidad, leer flash ni
flashear. No se intentó eludir la restricción mediante otra herramienta.

La resolución DNS de `pypi.org`, `github.com` y `registry.platformio.org` sigue
fallando. No se repitieron descargas que dependían de esa conectividad. La ruta
PlatformIO permanece configurada, pero sin build real verificado.

La validación en dispositivo continúa pendiente: panel, PSRAM real, tacto,
tearing, underrun, FPS y sesión de juego de 30 min. El perfil de benchmark ya
compilado y las instrucciones de captura están en el [README](../../voxel_firmware/README.md).
