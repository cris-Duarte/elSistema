# Design system — Tetris

## Identidad

Arcade original de 8 bits a color. Fondo negro, celdas perfectamente cuadradas, bordes
pixelados y contraste alto. No reutilizar logotipos, ROM, melodías ni sprites
de otros juegos. Los colores distinguen piezas, no rango de misión ni XP.

| Elemento | Color |
|---|---|
| Fondo | `#000000` |
| Celda vacía | `#0A101C` |
| Cuadrícula | `#182335` |
| I | `#35E8F3` |
| O | `#FFE14F` |
| T | `#B979FF` |
| S | `#46E874` |
| Z | `#FF5A69` |
| J | `#4B85FF` |
| L | `#FFAC48` |

## Composición

- Vertical fija 720×1280, independiente de la inclinación física.
- Entrada: título TETRIS, récord, explicación de gestos y botones amplios.
- Juego nuevo: 12 columnas y 20 filas, tablero de ancho completo de
  720×1200 px bajo una cabecera de 80 px. Cada celda mide exactamente 60×60 px;
  el renderizador recibe un solo tamaño para impedir deformaciones.
- Las partidas anteriores conservan sus 10 columnas y se dibujan centradas
  en 600×1200 px, también con cuadrados de 60 px. Nueva partida utiliza 12
  columnas; continuar nunca modifica los bloques de una partida guardada.
- Puntuación, récord, nivel, líneas, próxima pieza, aviso de nivel y Pausa
  quedan arriba. No hay columna lateral ni ayuda permanente sobre el tablero;
  las instrucciones se conservan en la pantalla de entrada.
- Ocultar la barra global de conectividad/batería durante el módulo.
- Pausa visible en la cabecera; el menú pausado proporciona Inicio. Todos
  los botones tienen al menos 48 px de alto; Pausa mide 92×52 px y las
  acciones de los menús conservan al menos 64 px de alto.
- Tipografía pixelada original 5×7 ampliada a 28 px para títulos y entrada.
  Rajdhani, con los caracteres españoles existentes, para ayuda, controles
  e indicadores compactos de 20 px durante la partida. Nivel y líneas
  aparecen en dos renglones, sin abreviaturas ambiguas.
- Icono compacto violeta con bloques dibujados mediante código, igual de
  accesible que los demás iconos del lanzador.

## Movimiento y sonido

Redibujado incremental a unos 33 Hz; no reconstruir el tablero por fotograma.
Destello de líneas de 200 ms, sin sacudidas ni zoom. Aviso de nivel discreto,
sin cubrir el tablero. La pausa y los diálogos congelan el juego, no las misiones.

Onda cuadrada y bajo triangular para música original; efectos cortos con
envolventes de entrada/salida. Volumen moderado y silenciamiento independiente.
Evitar clipping, colas de sonido y competencia con voces o micrófonos.

## Accesibilidad y límites

Los gestos no dependen del IMU. El menú explica cómo girar y acelerar. Nunca
interpretar un deslizamiento como toque al soltarlo. No colocar botones encima
del tablero en curso, salvo diálogos que hayan pausado previamente la sesión.

Objetivos físicos: ≥30 FPS y respuesta táctil <100 ms. Medir en Tab5;
la frecuencia del temporizador de UI no demuestra por sí sola los FPS.
