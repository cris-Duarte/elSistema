# Cámara — monitor USB para Sony ZV-E10 II

## Estado de la entrega

Primera entrega técnica, **no aceptación del módulo completo**. Añade el quinto
icono de Inicio (ID 10), transporte UVC 2.6.0, JPEG hardware del P4 y monitor
LVGL horizontal a 1280×720. Conserva ESP-IDF 5.4.2 y ESP-Hosted 1.4.1.

Pendiente: probar una Sony real antes de completar la interfaz. No se dan por
demostrados la grabación 4K simultánea, la fidelidad de color, los FPS o la
latencia. No se han añadido todavía guías, espejo, ampliación ni preferencias
NVS. No avanzar a esas funciones hasta aprobar el hito físico siguiente.

No se utiliza Wi-Fi, MacBook, micrófono, audio, tarjeta ni almacenamiento de
imágenes. No hay control de REC, ISO, diafragma, AF ni estado de la batería Sony.
El indicador «En vivo» solo significa imagen recibida, nunca grabación activa.

## Preparación y prueba física obligatoria

1. Alimentar la Tab5 y la cámara con sus propias baterías o fuentes adecuadas.
   El monitor no pretende cargar ni alimentar la Sony. El control de 5 V del
   USB-A permite la conexión USB; no garantiza un límite de corriente de 500 mA.
2. Usar un **cable de datos USB-A → USB-C**, del Host de Tab5 al puerto de Sony.
   El USB-C de Tab5 sigue reservado para alimentación/depuración durante la prueba.
3. En Sony: `USB Connection Mode → USB Streaming`, salida `HD (720p) 30p` o
   `25p`, `Movie Rec During Streaming → Enable`, `USB Power Supply → Off`.
4. Configurar grabación interna 4K a 25/30 fps según la toma habitual.
   Picture Profile no está disponible con USB Streaming. Consultar el
   [manual del modelo exacto ZV-E10M2](https://helpguide.sony.net/ilc/2430/v1/en/contents/211h_usb_streaming.html).
5. Abrir Cámara. «Conexión» muestra los pasos y formatos anunciados. El sistema
   solo acepta MJPEG 1280×720 a aproximadamente 25/30 fps (incluye 29,97).
   No baja silenciosamente de resolución ni selecciona otro formato.
6. Iniciar y detener REC **en la cámara**. Revisar después los archivos y sus
   propiedades en la tarjeta para confirmar 4K/25 o 4K/30 real y continuidad.
   No basta con ver «En vivo» en Tab5 para considerar aprobada esa prueba.
7. Mantener 30 minutos y luego una hora. No cambiar ajustes de protección
   térmica ni el watchdog para hacer pasar la prueba.

Los controles se muestran al tocar y se ocultan a los cinco segundos con señal
válida. En diagnóstico de conexión o sin señal permanecen disponibles. Inicio,
Conexión, Reintentar y Ocultar tienen áreas de al menos 48 px. El panel de
conexión se cierra con el mismo botón. Salir, apagar la pantalla y volver
detienen/reabren el stream; no se envían órdenes de grabación a la Sony.

## Arquitectura y propiedad de memoria

- `UsbHostRuntime` es el propietario único del Host. Su pequeña tarea de eventos
  vive hasta reiniciar para poder compartirlo con futuros clientes. No se llama
  al inicializador USB del BSP, que contiene abortos y un cierre no coordinado.
- `CameraUsbTransport` instala UVC únicamente dentro del módulo, publica avisos
  de conexión y retiene como máximo un MJPEG pendiente. Un cuadro más reciente
  sustituye al pendiente, devolviendo el anterior al driver. Los callbacks no
  decodifican, no asignan grandes buffers y no invocan LVGL.
- El worker de `CameraService` enumera formatos y negocia, procesa JPEG y entrega
  snapshots. Tres buffers MJPEG de 1 MiB y dos RGB565 alineados en PSRAM consumen
  aproximadamente 6,52 MiB, más 32 KiB de URBs y las estructuras del controlador.
  El presupuesto de video es 8 MiB y se comprueba también RAM interna.
- Se validan tamaño/segmentos JPEG, resolución, codificación baseline, tablas y
  componentes antes del decoder. MJPEG sin DHT usa las tablas estándar del SDK.
  Se rechazan resoluciones/codificaciones no soportadas. Tras 30 errores
  consecutivos se detiene el stream con diagnóstico recuperable.
- Los slots RGB tienen estados libre, escribiendo, listo y mostrado. Nunca se
  decodifica sobre el mostrado. LVGL cambia la fuente y descarta su caché antes
  de liberar el slot anterior. La limpieza espera a que LVGL devuelva sus leases.
- Primero se detienen callbacks, se devuelven cuadros pendientes y se cierra el
  stream; después se desinstala UVC y se apaga USB5V. Los buffers de video se
  liberan solo sin lectores. Si el driver no puede cerrarse con seguridad,
  se informa y exige reinicio; no se reinstala a ciegas.
- Una sesión identifica cada apertura y descarta RGB atrasado. Tras un segundo
  sin cuadro fresco se cubre la imagen con «Sin señal». No hay un falso REC.
- La pantalla, PPA, táctil, fuentes y checkpoints de misiones siguen usando
  los servicios existentes. Recordatorios se superponen sin abandonar Cámara.
  La orientación se fija en horizontal y se restaura según el IMU al salir.

El tamaño de descriptor de control USB se amplía de 256 a 4096 B para dispositivos
compuestos; se conservan hasta 16 intervalos por formato. La dependencia y sus
licencias las gestiona el Component Manager. La PSRAM, el BSP de pantalla y las
particiones no se sustituyen.

## Instrumentación y criterios

La cabecera informa FPS negociados, FPS decodificados y cuadros entregados a
LVGL («vista»), descartados, tiempo de decoder y bytes reservados. **«Vista» no
es una medición física del panel ni de latencia de extremo a extremo**.
El log `camera` añade errores USB, recibidos, inválidos y memoria libre cada 5 s.
Los logs no contienen imágenes.

Medir latencia mostrando un cronómetro o referencia visual a la Sony y
fotografiando simultáneamente la referencia y su vista en Tab5. Objetivo ideal
menor a 200 ms. Controles: menos de 100 ms de respuesta. Cadencia: sostener la
fuente 25/30 fps sin acumulación de retraso. No atribuir al monitor los FPS de Inicio.

| Comprobación | Estado |
| --- | --- |
| Modelo portable, leases, sesiones, antigüedad, intervalos y límites JPEG | Pruebas nativas pasadas con ASan/UBSan (20/10/2026) |
| Compilación ESP-IDF 5.4.2 + UVC 2.6.0 | Compilación principal satisfactoria (20/10/2026) |
| Sony anuncia/negocia 720p MJPEG a 25 y/o 30 | Pendiente hardware |
| Color, proporciones, movimiento y detalle | Usuario confirma recepción con colores alterados; corrección de byte order preparada, pendiente repetir |
| 4K interno simultáneo y continuidad al iniciar/detener REC | Pendiente hardware |
| 30 minutos y una hora sin fugas/reinicios | Pendiente hardware |
| Diez reconexiones/entradas/salidas y apagado/despertar | Pendiente hardware |
| Latencia óptica, FPS físicos y respuesta táctil | Pendiente hardware |
| Convivencia con Wi-Fi, dictado, recordatorios y otros módulos | Pendiente hardware |

Pruebas locales: `sh tools/tests/probar_camara.sh` (AddressSanitizer y UBSan).
También pasaron las pruebas nativas de Inicio, Tetris/audio, Hábitos y limpieza
de módulos retirados. No equivalen a una validación física del monitor.
Los fixtures de cabecera JPEG prueban estructura y límites, no reemplazan una
captura real ni una prueba del decoder hardware.

### Corrección de color (20/10/2026)

La primera prueba del usuario recibió imagen con colores azules/rojos alterados.
Se encontró `JPEG_DEC_RGB_ELEMENT_ORDER_RGB` en la salida RGB565: en IDF 5.4.2
ese valor selecciona **big endian** mediante el intercambio de bytes del DMA.
LVGL consume RGB565 como `uint16_t` nativo (little endian en el P4). Se cambia
solo el decoder de Cámara a `JPEG_DEC_RGB_ELEMENT_ORDER_BGR`, que en este API
selecciona little endian, sin intercambiar canales ni tocar el LCD global.
No añade un recorrido CPU, otra imagen ni memoria por fotograma.

El contrato tiene una prueba compilada con los tipos reales del SDK. No demuestra
por sí sola la fidelidad de la imagen. Falta cargar la revisión y comparar rojo,
verde, azul y un objeto blanco/gris entre Sony y Tab5; después comprobar piel,
movimiento y grabación interna. No se da por aprobada la prueba física.

Si falla grabación interna o rendimiento, registrar formatos, memoria y síntomas
y **detener la ampliación**. No añadir capturadora ni actualizar SDK sin acordarlo.

## Segunda entrega, condicionada a aprobar la prueba

Cuadrícula de tercios, centro, guías 16:9/9:16/1:1 sin recorte, espejo horizontal,
ampliación 1×/2×/4× con desplazamiento/centrado y etiqueta de ampliación. Solo
preferencias visuales en NVS propio y ampliación a 1× al entrar. Ninguna ayuda
modificará el video guardado por Sony. Quedan fuera histograma, zebras, peaking,
LUT, audio, grabación y controles propietarios de la cámara.
