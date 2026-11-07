# Tab5 Voxel — estado de implementación

Fecha: 7 de noviembre de 2026. Versión: 0.1.1.

## Resultado

El proyecto autónomo `voxel_firmware/` implementa las funciones del MVP: terreno
procedural, render DDA, jugador con AABB/gravedad/salto, controles multitáctiles,
slots, romper/colocar, pausa y diagnóstico. Tiene configuración PlatformIO/Arduino
y un build nativo ESP-IDF del mismo código.

La versión 0.1.1 corrige rayos que se perdían por redondeo al entrar en el mapa.
Incluye 18 000 rayos de regresión por las seis caras y una entrada por esquina,
además de un perfil autónomo de cuatro escenas y herramientas de captura/informe.
Compilan los perfiles normal, diagnóstico y benchmark; las pruebas ASan/UBSan
pasan en ambas resoluciones.

| Fase del plan | Código y verificación disponibles | Pendiente |
| --- | --- | --- |
| 0 · Viabilidad | Build ESP32-P4 con ESP-IDF 5.4.2; bibliotecas locales preparadas independientemente | Build PlatformIO y arranque en Tab5; identificar panel físico |
| 1 · Hardware | Backend DSI, dos buffers nativos, escalado/orientación, diagnóstico y timeout | Swap sin tearing, frecuencia PSRAM real, contactos y 10 min de patrón |
| 2 · Mundo | PSRAM, Perlin entero, bloques, capas, árboles y spawn; pruebas host | Reserva y consumo reales tras arrancar |
| 3 · Cámara | Base de perspectiva, FOV y entrada; pruebas host | Sensibilidad y orientación física del tacto |
| 4 · Render | DDA, sombreado, límites, RGB565 y perfiles; pruebas host y preview | ≥20 FPS en las escenas del plan |
| 5 · Física | AABB, pasos fijos, salto, suelo/techo/pared; pruebas host | Sensación de movimiento durante juego físico |
| 6 · Juego | Selección, alcance, edición, slots, menú y cancelación de contactos | Sesión 30 min y validación de acciones simultáneas |

Las condiciones físicas de avance no se dan por cumplidas. Se prepararon los
módulos de dominio independientes mientras la validación del equipo está pendiente.

## Decisiones de integración

PlatformIO no estaba instalado. El intento de instalar PlatformIO Core 6.1.18
falló al resolver `pypi.org`; no se solicitó una escalación de permisos. Para
obtener una compilación real se usó ESP-IDF 5.4.2 ya instalado y copias de
M5Unified 0.2.25 y M5GFX 0.2.32 disponibles localmente. Se conservó la ruta
PlatformIO solicitada, pero se distingue de la ruta que sí se compiló.

El backend conserva a M5Unified/M5GFX como único inicializador de hardware.
Un getter pequeño y verificado en `Panel_DSI.hpp` expone el handle DPI para usar
las APIs nativas del SDK. El parche se aplica solo a los componentes del nuevo
proyecto, sin tocar el firmware previo. La procedencia y hashes de los árboles
locales se registran en `components/sources.json` del proyecto preparado.

El escalado inicial es CPU nearest-neighbor, sin PPA. Se priorizó conservar
bordes pixelados y disponer de una transformación comprobable. El coste de
presentación y el posible backend PPA se evaluarán con mediciones físicas.

## Validación reproducible

Instrucciones completas y comandos en [README del firmware](../../voxel_firmware/README.md).
El informe de 0.1.0 se conserva en `validacion.md`; la continuación está en
[validacion-0.1.1.md](validacion-0.1.1.md).

El equipo aparece en `/dev/cu.usbmodem101`, pero el intento de `chip_id` fue
rechazado al abrir el puerto con `Operation not permitted`. No se leyó la
identidad ni se hizo backup, escritura o flasheo durante esta continuación.
La resolución DNS de servidores de paquetes sigue sin estar disponible, por
lo que la ruta PlatformIO tampoco pudo completarse.
