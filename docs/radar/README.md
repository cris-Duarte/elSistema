# Radar — Wi-Fi y Bluetooth LE

## Función y límites

Cuarto módulo local de El Sistema. Explora puntos de acceso Wi-Fi de 2,4 GHz
y anuncios Bluetooth LE, y permite seguir la intensidad de un emisor mientras
se mueve la Tab5. No necesita conexión a una red ni servidor de Internet.

El círculo **no es un mapa**: la distancia al centro representa RSSI suavizado;
los ángulos son decorativos y estables por identificador. Paredes, antenas,
potencia e interferencias afectan la señal. No mide metros ni direcciones y
no garantiza encontrar todos los teléfonos. No soporta Wi-Fi de 5 GHz ni
Bluetooth clásico.

## Uso

1. Abrir RADAR en Inicio; ambos escaneos empiezan automáticamente.
2. Filtrar TODOS / WI-FI / BLE. Los filtros afectan la vista, no el escaneo.
3. Tocar un punto o una fila para seguir un emisor. Si coinciden varios puntos,
   elegir en el diálogo ampliado.
4. Consultar RSSI actual, media, tendencia, última detección y gráfica de 60 s.
5. PAUSAR detiene los escaneos; REANUDAR abre otra sesión de observación.
6. SONIDO activa pitidos opcionales de seguimiento. INICIO sale del módulo.

El círculo muestra hasta 24 emisores, priorizando señal e incluyendo el
seleccionado si pasa el filtro. La lista permite consultar todos los retenidos
en páginas de 12. Wi-Fi se identifica por BSSID, no por SSID. BLE se identifica
por dirección y tipo; no se vinculan direcciones privadas cambiantes. Nombres
no anunciados: `Red oculta` o `BLE sin nombre`.

## Servicios

- Wi-Fi: solicitud cada 5 s. Conexión, reconexión y comandos explícitos tienen
  prioridad. El worker existente de DictationService sigue siendo la única
  tarea propietaria de operaciones Wi-Fi y llama a ConnectivityService. Los
  resultados completos llegan antes del agrupamiento por SSID de configuración.
- BLE: observación activa, intervalo 100 ms, ventana 30 ms, repetidos
  habilitados. Se conservan anuncio y GATT locales. El callback copia a una
  cola de 64 registros; el worker del Radar interpreta nombres y actualiza
  el modelo. No se conecta ni empareja con emisores.
- Cambiar sesión invalida respuestas tardías. Salir o apagar pantalla cancela
  BLE y solicitudes Wi-Fi pendientes. Un escaneo Wi-Fi ya iniciado puede
  terminar, pero su resultado se descarta y no se programa otro.
- RSSI: EMA 0,25. Tendencia: medias de los últimos 5 s y ventana anterior hasta
  15 s, umbral 5 dB; sin suficientes datos se muestra `Midiendo tendencia`.
- A los 15 s sin detección, se atenúa el punto. A los 60 s se retira, salvo el
  seleccionado. Tras ausencia larga se reinicia la media de señal.
- Máximo 128 emisores; se sustituye primero el no seleccionado más antiguo
  y se informa LISTA LIMITADA.
- Fondo precalculado y dos canvas RGB565 en PSRAM. Solo LVGL presenta imágenes
  y modifica widgets. Los halos y el barrido restauran regiones acotadas del
  fondo; se limita a 12 regiones para reservar invalidaciones de LVGL a los
  controles. Cada búfer se compara con su propio fotograma anterior para no
  dejar estelas. Objetivo: 30 FPS.
- Pitidos de 1,1 kHz, intervalos 2 s–400 ms según intensidad. AudioSessionManager
  omite el pulso si el codec está ocupado. No acumula sonidos pendientes ni
  reproduce señales desactualizadas. Una solicitud de voz cancela el pitido
  y espera su liberación en el worker; nunca bloquea la interfaz.
- Misiones y checkpoints continúan funcionando.

## Persistencia y privacidad

NVS `radar`, versión 1: exclusivamente filtro y sonido. Primera instalación:
TODOS y silencio. Detecciones, direcciones, selección y gráfica viven en RAM
hasta reiniciar; no se escriben en SPIFFS, NVS ni logs de aplicación. No hay
favoritos, exportación, captura de tráfico ni geolocalización.

Se conservan ESP-IDF 5.4.2, ESP-Hosted 1.4.1 y firmware del C6.

## Verificación

`sh tools/tests/probar_radar.sh` comprueba identidad, RSSI, tendencias, límites,
caducidad, sesiones, anuncios BLE, filtros, colisiones y memoria del raster.
También valida las composiciones de 720×1280 y 1280×720, los colores del fondo
plano y la cuadrícula, y 100 fotogramas incrementales del disco de 720 px.
Acepta una ruta PPM opcional para inspeccionar el mismo dibujo usado en la placa.

El build de pruebas agrega casos Unity y un benchmark sin flush. Con
`-DEL_SISTEMA_TESTS=ON -DEL_SISTEMA_RADAR_SMOKE=ON`, abre Radar durante 30 s,
informa cantidades Wi-Fi/BLE y conexión, y vuelve a Inicio verificando el
cierre. Esta instrumentación no se incluye en la compilación normal.

### Resultado en Tab5 — 07/10/2026

Estas mediciones corresponden a la composición anterior (discos de 480/520 px).
El ajuste visual posterior amplía el disco a 720 px, con verde plano, cuadrícula
negra y controles fuera del círculo. Su fluidez y distribución táctil deben
volver a comprobarse en la placa; no se extrapolan los FPS anteriores.
El firmware normal del ajuste compila con ESP-IDF 5.4.2 y las suites portables
de Radar, Inicio y Reloj de arena pasan. El ajuste se flasheó junto con la
corrección que habilita la carga de batería; su fluidez sigue pendiente de
medición en la placa.

- ESP-IDF 5.4.2: compilación de producción y de pruebas correctas.
- 39 pruebas Unity, ninguna fallida; suite portable de Radar y comparación
  de 100 fotogramas incrementales con el render completo también correctas.
- Raster completo 520×520 / 24 puntos: 9,17 ms por fotograma, sin flush LVGL.
- Composición real del módulo: 31,5 FPS en 1280×720 y 32,1 en 720×1280.
- Observación real: 24 emisores Wi-Fi y 24 BLE, sin informes descartados de la
  cola BLE en esta prueba. Se obtuvo y mantuvo conexión Wi-Fi durante el scan.
- Al volver a Inicio, sesión inactiva y observación BLE detenida, sin reinicios.

Aceptación manual pendiente: cliente GATT conectado durante escaneo,
acercamiento/alejamiento, redes ocultas y direcciones privadas, toques
superpuestos, ambas orientaciones, pitidos con otros propietarios de audio,
ciclos repetidos de entrada/salida y transcripción posterior. El log de FPS
mide composición, no refresco físico del panel.

Guía visual: [design.md](design.md).
