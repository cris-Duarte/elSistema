# Tetris — módulo retro

Juego local de piezas descendentes, disponible como quinto icono de Inicio.
No requiere red, no concede XP y no modifica misiones. La misión activa y
sus checkpoints siguen funcionando mientras se juega.

## Uso

- El módulo bloquea la pantalla en vertical de 720×1280. Volver a Inicio
  recupera la orientación detectada por el IMU.
- El tablero ocupa los 720 px de ancho y mantiene sus 20 filas visibles.
  Puntos, récord, nivel, líneas, próxima pieza y Pausa están en la cabecera.
  Las celdas miden 60×60 px: las partidas nuevas tienen 12 columnas y 20 filas.
  Las partidas anteriores continúan con sus 10 columnas, centradas y sin
  deformaciones; iniciar una nueva permite usar el tablero ampliado.
- Nueva partida comienza desde nivel 1 y cero puntos. Continuar recupera una
  partida pausada. Sustituir una partida sin terminar requiere confirmación.
- Tocar el tablero gira en sentido horario. Deslizar lateralmente mueve una
  columna por cada celda recorrida. Arrastrar hacia abajo al menos 12 px
  acelera la caída a una celda cada 40 ms mientras se mantiene el contacto.
- Un arrastre no genera un giro al soltarlo. Los movimientos imposibles se
  rechazan, sin acumular desplazamientos ni acomodar giros contra paredes.
- Pausa abre Reanudar, Nueva partida, Sonido e Inicio. Salir, apagar la
  pantalla o recibir un recordatorio pausa el juego; continuar es explícito.
- Un recordatorio se presenta después de que el worker confirme la pausa.
  Cerrar o atender el aviso nunca reanuda automáticamente la partida.

## Reglas

Tablero de 12×20, con dos filas internas ocultas de entrada. Siete piezas con
selección aleatoria independiente, una próxima pieza visible; sin reserva,
fantasma, caída instantánea, combos ni bonificaciones modernas.

La pieza se fija al encontrar un obstáculo al descender. Las filas completas
se eliminan simultáneamente tras un destello de 200 ms, durante el cual se
descartan gestos y se congela la caída. La partida termina si la nueva pieza
no puede aparecer o quedan bloques en las filas ocultas después de eliminar.

Cada diez líneas aumenta el nivel. Intervalo de caída:
`max(80, floor(800 × 0,85^(nivel−1)))` ms.

| Filas simultáneas | Puntos × nivel anterior |
|---|---:|
| 1 | 40 |
| 2 | 100 |
| 3 | 300 |
| 4 | 1200 |

El descenso manual concede un punto por celda, no por fijación. El récord
se conserva al comenzar una nueva partida.

## Sonido

Melodía chiptune original y efectos sintetizados para desplazamiento, giro,
fijación, líneas, nivel y final. PCM mono de 16 kHz, mezclado en un worker;
no hay grabaciones, recursos de ROM ni trabajo de audio en LVGL.

Música y efectos se activan inicialmente, volumen 35/100. Son configurables
independientemente; volumen cero permite jugar en silencio. El audio del
juego tiene baja prioridad: cede a captura, Asistente y avisos, descarta
eventos con más de 100 ms y no reproduce una cola atrasada al recuperarse.
Un fallo del codec no impide jugar.

## Arquitectura y persistencia

- `TetrisEngine`: reglas portables y estado determinista, sin LVGL/FreeRTOS.
- `TetrisGesture`: interpretación portable del contacto, sin lógica de juego.
- `TetrisService`: cola de órdenes acotada, pausa y fin de contacto prioritarios,
  reloj monotónico, snapshots y worker separado de audio.
- `TetrisRenderer`: hasta 240 celdas cuadradas reutilizadas; únicamente cambia las que difieren.
  Usa el framebuffer y la gestión de pantalla existentes, sin otro driver.
- `TetrisRepository`: espacio NVS `tetris`, registros little-endian versionados
  con CRC32. Partida y preferencias/récord tienen registros independientes.

Checkpoint cada 30 s en curso y al comenzar, reanudar, pausar, salir, terminar
o ajustar sonido. Se conserva tablero, pieza, próxima pieza, generador
aleatorio, puntos, líneas, nivel y tiempo pendiente, incluida una eliminación
de filas en progreso. Una sesión activa se restaura pausada, sin tiempo apagado.
Un corte inesperado puede perder hasta el último intervalo de checkpoint.

El registro de partida v2 conserva el número de columnas. El lector admite
los registros v1 de 280 bytes sin alterar su tablero, pieza ni eliminación
pendiente; al guardar se actualiza el formato, no las reglas de esa sesión.
Récord y preferencias mantienen su formato independiente y no se borran.

Los datos dañados se rechazan con aviso. Un récord válido se mantiene aunque
se descarte la sesión. Si NVS falla, el juego sigue en RAM y muestra que no
puede guardar; no borra datos de otros módulos y reintenta la escritura.

## Validación

`sh tools/tests/probar_tetris.sh` verifica piezas y giros, paredes sin kicks,
filas de 1 a 4, puntuación y niveles, pausas, recuperación durante el destello,
CRC, ajustes, fin de partida, gestos y síntesis sin clipping. Incluye 100.000
pasos de simulación y 30 ciclos concurrentes de prioridad del árbitro de audio.
También verifica colisiones y filas en 10 y 12 columnas, lectura de partidas
v1, checkpoints v2, geometría cuadrada y uso de las dos columnas adicionales.

Las pruebas Unity incluyen reglas, recuperación y arbitraje del audio.
El smoke test LVGL de escritorio comprueba fuentes y renderizado incremental.
Para ejecutarlo:

```sh
cmake -S tools/tests/tetris_lvgl -B /tmp/elsistema-tetris-lvgl-tests
cmake --build /tmp/elsistema-tetris-lvgl-tests -j 8
/tmp/elsistema-tetris-lvgl-tests/tetris_lvgl_smoke
```

Aceptación pendiente en la Tab5 real: gestos rápidos y bordes, continuidad de
misiones, avisos, apagado/reinicio, reproducción y prioridad de audio, ausencia
de fugas en entradas/salidas y al menos 30 FPS con respuesta inferior a 100 ms.
Los logs de frecuencia de actualización UI no sustituyen esa medición física.

## Límites

Reglas clásicas propias, no emulación de una edición concreta. Sin red,
multijugador, clasificación en línea ni controles por inclinación.
