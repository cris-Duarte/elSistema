# Laberinto — juego de bola por inclinación

Sexto módulo de El Sistema. Juego original, local y silencioso, con tres
laberintos deterministas, bola metálica, paredes, agujeros y una meta verde.
No utiliza red, micrófonos ni XP. Productividad mantiene su cronómetro y
checkpoints mientras se juega.

## Uso

1. Abre LABERINTO desde Inicio y elige un nivel.
2. Coloca la Tab5 casi horizontal, con la pantalla hacia arriba, y toca
   INICIAR Y CALIBRAR. Mantenla quieta durante unos 600 ms.
3. Inclínala suavemente: el plano del acelerómetro mueve la bola respecto
   de la posición neutra calibrada. No se añade gravedad artificial.
4. Evita los agujeros oscuros y alcanza la meta verde. El tiempo se detiene
   al perder la bola o completar el nivel.

El nivel 1 tiene dos agujeros; el 2, cuatro; el 3, seis. Los mapas se generan
con semillas fijas y tienen rutas transitables por una bola de 14 px de radio.
Los agujeros se pueden rodear. No se necesita tocar el tablero para jugar.

PAUSA ofrece reanudar, calibrar de nuevo sin borrar el tiempo, reintentar
desde la salida y seleccionar un nivel. La recalibración requiere otra vez
una posición casi horizontal y quieta. Cambiar de nivel reinicia esa sesión.
Después de ganar, Siguiente nivel abre la preparación del siguiente; no
comienza a mover la bola automáticamente.

Salir, apagar la pantalla o recibir un recordatorio de Productividad pausa
el juego. Cerrar un recordatorio o regresar no reanuda automáticamente.
Inicio y Pausa permanecen accesibles; la pantalla del juego está bloqueada
en vertical 720×1280. Al volver a Inicio se recupera la orientación física.

## Arquitectura

- `MazeEngine`: estados Listo, Calibrando, En curso, Pausado, Bola perdida y
  Completado; tiempo monotónico, calibración, agujeros y récords.
- Generador portable de mapas mediante recorrido en profundidad: 6×9
  habitaciones de 100 px, conexiones transitables y paredes de 8 px.
- Física de círculo contra rectángulos, fricción, restitución atenuada,
  límite de velocidad y subpasos de como máximo 4 ms. Los agujeros y la meta
  se comprueban durante los subpasos, no solo al terminar el fotograma.
- `MazeService`: trabajador FreeRTOS, cola de acciones acotada, pausa
  prioritaria y snapshots. Consume muestras del propietario existente del
  IMU; nunca lee el hardware ni manipula LVGL desde la interfaz.
- `MazeRenderer`: paredes, agujeros y meta estáticos; mueve un objeto de
  bola reutilizado. Usa los buffers de pantalla existentes, no otro driver.
- NVS propio `maze`, registro versionado con CRC para nivel seleccionado
  y mejor tiempo por cada uno de los tres niveles. Escrituras fuera de LVGL.

La sesión se conserva en RAM al cambiar de módulo. Tras reiniciar el equipo
se conserva el nivel y sus récords, pero no una partida a medias. Un error de
NVS permite seguir jugando y muestra que el récord solo está en RAM; no se
borran datos de otros módulos. Los registros inválidos se rechazan.

Si el IMU deja de entregar datos válidos, el juego se pausa y explica el
motivo. La recuperación exige reanudación explícita. Si hay una demora del
sistema mayor de 250 ms, se pausa en vez de simular un salto de la bola.

## Validación

```sh
sh tools/tests/probar_laberinto.sh
cmake -S tools/tests/maze_lvgl -B /tmp/elsistema-maze-lvgl
cmake --build /tmp/elsistema-maze-lvgl -j 8
/tmp/elsistema-maze-lvgl/maze_lvgl_smoke
```

Pruebas portables: aceleración, fricción, límites, 15.000 pasos contra
paredes, pausa, sensor inválido, calibración inestable, caída/reintento,
CRC y composición de los seis iconos. Un controlador simulado recorre los
tres mapas evitando obstáculos y alcanza sus metas sin teletransportar la bola.
Las pruebas LVGL comprueban límites y conservación de objetos en los tres mapas.

Pendiente de aceptación en la Tab5 real: sentido de ambos ejes, sensibilidad
y estabilidad de la calibración, comodidad con batería instalada, pausas por
recordatorios/apagado, persistencia de récords y fluidez/tacto. El refresco
de UI a unos 33 Hz no demuestra por sí solo los FPS físicos.

Posibles ampliaciones: efectos sonoros originales, selector de sensibilidad,
más mapas y recuperación de partida tras reinicio. No forman parte de esta versión.
