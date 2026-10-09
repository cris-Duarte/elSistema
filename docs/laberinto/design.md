# Design system — Laberinto

## Identidad

Laberinto de sobremesa original: superficie oscura de madera estilizada,
paredes cálidas, bola metálica y meta verde. Fondo negro para separar
claramente tablero y controles. Todo se dibuja mediante código; no hay
texturas, sprites ni recursos de juegos comerciales.

| Elemento | Color |
|---|---|
| Fondo | `#000000` |
| Tablero | `#241C14` |
| Paredes | `#A87843` |
| Bordes de pared | `#D0A169` |
| Bola / reflejo | `#B9C8D5` / `#F5FAFF` |
| Agujeros | `#070707` |
| Meta | `#1B725A` / `#61E2AC` |
| Acento del módulo | `#E4AA60` |

## Composición y controles

- Vertical fija de 720×1280. La inclinación modifica solo el movimiento
  de la bola, nunca la orientación de texto, tablero ni menús.
- Cabecera: LABERINTO, Inicio, nivel, cronómetro e intentos. Rajdhani de
  40/28/20 px, con caracteres españoles, compartida con los otros módulos.
- Tablero de 600×900 px en x=60, y=170; bola de 28 px de diámetro,
  agujeros de 36 px y meta de 52 px. No deformar la bola.
- Pausa de 600×64 px debajo del tablero durante juego/calibración.
  Inicio mide 152×52 px. Acciones de menú de al menos 60 px de alto.
- Menús sobre una superficie oscura translúcida; la física se encuentra
  detenida antes de mostrar pausa, errores o recordatorios.
- Meta verde y agujeros negros difieren en forma/borde, no solo en color.
  La calibración y los problemas del sensor siempre tienen una explicación textual.
- Icono de Inicio: pasillos y bola dibujados sobre un cuadrado ámbar,
  manteniendo el sistema de iconos compactos y las filas centradas.

## Movimiento

Sin sacudidas de cámara, barridos ni animaciones que oculten la bola. Física
en trabajador independiente y representación incremental a unos 33 Hz.
Objetivo de prueba física: al menos 30 FPS y controles de pausa sin bloqueos.
La versión inicial no emite sonidos ni compite con dictado o Asistente.
