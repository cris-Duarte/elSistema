# Respaldo y restauración de la Tab5

## Respaldo más reciente: 7 de noviembre de 2026

La copia actual de la Tab5 está en `backups/tab5-2026-11-07/` y contiene los
16 MiB de flash del ESP32-P4. Se verificó contra el dispositivo completo con
`verify_flash` (`digest matched`). También se guardaron por separado NVS y
SPIFFS, junto con `manifest.json`, `SHA256SUMS` y el registro de verificación.
Corresponde a la Tab5 con MAC `e8:f6:0a:e2:ea:62`. La aplicación respaldada
se identifica como `el_sistema` v0.4.2, compilada el 20 de octubre de 2026.

Para restaurar exactamente esta copia en la **misma** Tab5, comprobar primero
la identidad y el puerto serie actual. Desde la raíz del proyecto:

```bash
cd backups/tab5-2026-11-07 && shasum -a 256 -c SHA256SUMS && cd ../..
~/.espressif/python_env/idf5.4_py3.14_env/bin/python -m esptool \
  --chip esp32p4 --port /dev/cu.usbmodem101 --baud 460800 \
  write_flash 0x0 backups/tab5-2026-11-07/flash-completa.bin
```

El puerto puede cambiar. Esta restauración reemplaza firmware y datos actuales
por los existentes el 7 de noviembre. No incluye la flash del coprocesador
ESP32-C6 ni una microSD.

Ese mismo día se instaló el firmware oficial **Tab5 User Demo** en esta Tab5,
después de crear y verificar el respaldo anterior. La imagen instalada se
verificó contra la flash y el arranque confirmó `m5stack_tab5` versión
`639d9cc`, apertura de `AppLauncher` e inicio de Wi-Fi. Los registros de esta
instalación están en `factory/tab5/instalacion-2026-11-07.json`.

## Respaldo anterior: 12 de octubre de 2026

El respaldo del 12 de octubre de 2026 está en `backups/tab5-2026-10-12/`.
Corresponde a la Tab5 con ESP32-P4, revisión v1.3 y MAC
`e8:f6:0a:e2:ea:62`. Contiene una imagen de los 16 MiB de flash, las
particiones de datos por separado, `manifest.json`, `SHA256SUMS` y el registro
de verificación. La imagen completa fue comparada con la flash del dispositivo
mediante el MD5 calculado por esptool en el chip: `verify OK (digest matched)`.

El 12 de octubre de 2026 se instaló en esta Tab5 el firmware oficial
**Tab5 User Demo** de M5Stack (aplicación `m5stack_tab5`, versión `639d9cc`).
La imagen instalada quedó en `factory/tab5/tab5-user-demo.bin`; se verificó
la escritura en flash y se observó el arranque de la aplicación y de Wi-Fi.
El respaldo de nuestro firmware v0.4.2 permanece disponible para restauración.
Después de las pruebas, el 12 de octubre de 2026 se restauró la imagen
completa en la misma Tab5. La verificación de los 16 MiB coincidió con el
respaldo y el registro de arranque confirmó `el_sistema` v0.4.2.

La partición `nvs` (`0x9000`, 24 KiB) incluye configuración, perfiles Wi-Fi,
checkpoints y Tetris. La partición SPIFFS `storage` (`0xa10000`, 2 MiB) incluye
misiones, historial y hábitos. La imagen completa también guarda bootloader,
tabla de particiones, aplicación y regiones restantes de la flash del P4.

La copia puede contener contraseñas Wi-Fi y datos personales. `backups/` está
excluido de Git y sus archivos tienen permisos de lectura solo para el usuario.
Conservar otra copia privada fuera de esta carpeta protege ante pérdida del Mac.

## Antes de flashear

Comprobar desde la carpeta `backups/tab5-2026-10-12/`:

```bash
shasum -a 256 -c SHA256SUMS
```

La lectura de flash reinicia el dispositivo. Solo representa el estado ya
persistido cuando se hizo la copia; no incluye ediciones sin guardar, RAM,
PSRAM, registros del RTC, microSD, eFuses ni la flash del coprocesador ESP32-C6.

## Restaurar exactamente el estado de la flash del P4

Comprobar primero que la Tab5 conectada sea la misma y elegir su puerto serie.
Desde la carpeta raíz del proyecto:

```bash
~/.espressif/python_env/idf5.4_py3.14_env/bin/python -m esptool \
  --chip esp32p4 --port /dev/cu.usbmodem2101 --baud 460800 \
  write_flash 0x0 backups/tab5-2026-10-12/flash-completa.bin
```

Esta operación sobrescribe toda la flash del P4 y devuelve el firmware v0.4.2
y los datos al instante de la copia. El puerto puede cambiar. No aplicar la
imagen a otra Tab5: incluye datos y configuración específicos del dispositivo.

## Restaurar datos tras instalar nuestro firmware

Si el firmware instalado conserva exactamente la tabla de particiones de este
respaldo y entiende su formato de datos, se pueden restaurar solo `nvs` y
`storage`. Conviene comprobar esos requisitos antes de escribir. Desde la raíz:

```bash
~/.espressif/python_env/idf5.4_py3.14_env/bin/python -m esptool \
  --chip esp32p4 --port /dev/cu.usbmodem2101 --baud 460800 \
  write_flash 0x9000 backups/tab5-2026-10-12/nvs.bin \
              0xa10000 backups/tab5-2026-10-12/storage.bin
```

Después de restaurar, abrir misiones, hábitos, redes y Tetris en la Tab5 para
confirmar que los datos cargan. Una restauración posterior con esta copia
descarta los cambios hechos después del respaldo.
