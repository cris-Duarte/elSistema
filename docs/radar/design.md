# Design system — Radar

## Identidad

Instrumento lúdico verde sobre negro, inspirado en la referencia circular del
usuario. Dibujo propio generado con código; no incluye imágenes ni recursos de
la obra. No adopta el HUD de misiones ni concede XP.

| Elemento | Color / forma |
|---|---|
| Fondo | Negro / paneles verde muy oscuro |
| Disco y cuadrícula | Verde plano #00D070 / líneas negras #000000 |
| Marco | Marfil con marcas oscuras |
| Centro | Triángulo naranja |
| Wi-Fi | Punto amarillo con halo |
| BLE | Diamante cian con halo |
| Seleccionado | Contorno claro |
| Señal desactualizada | Halo atenuado |

Rajdhani para título, botones y filas; Poppins para metadatos y advertencias.
Controles de al menos 48 px. Los tipos se diferencian por forma y color; las
filas llevan W/B.

## Composición

- Cabecera RADAR, Inicio, filtros, pausa y sonido fuera del círculo. El estado
  de escaneo se integra en estos controles; no hay barra global sobre el disco.
- Indicación permanente **INTENSIDAD · NO UBICACIÓN** y leyenda.
- Vertical 720×1280: disco 720 px arriba, de ancho completo. Controles,
  seguimiento y lista paginada desplazable debajo.
- Horizontal 1280×720: disco 720 px a la izquierda, de alto completo;
  controles, seguimiento y lista desplazable a la derecha.
- Fondo del disco uniforme, sin degradados ni reflejos de cristal. Las líneas
  negras de la cuadrícula permanecen nítidas; los halos se reservan a emisores.
- El seguimiento permite desplazamiento vertical si sus metadatos no caben.
- Identificador y edad de medición siempre disponibles en seguimiento.
- Gráfica de 60 s con huecos donde no hay muestras: no inventar señal.

## Movimiento y honestidad visual

Barrido decorativo de seis segundos a unos 30 FPS. No crea detecciones ficticias
ni determina cuándo aparecen mediciones. El IMU interviene únicamente en la
orientación general de la interfaz, no en posiciones de emisores.

Radio normalizado 0,15–0,87 según RSSI medio, escala -100 a -35 dBm saturada.
Ángulo por hash de identidad. Zona táctil de puntos de 48 px; coincidencias
abren una selección ampliada. Sonido opcional, inicialmente apagado, sin
promesas de distancia o dirección.
