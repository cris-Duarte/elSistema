# Design system — Reloj de arena

## Propósito

El cristal, la arena y el tiempo transcurrido son todo el contenido durante
una sesión. El módulo invita a inclinar y voltear un objeto que permanece
anclado al panel; no gira para compensar el movimiento del dispositivo.

## Identidad

Fondo negro puro. Cristal curvo simétrico, dos cámaras, cuello estrecho y
soporte discreto. Arena densa hecha de pequeños núcleos de luz dorada con
intersticios oscuros, resplandor contenido y perfiles que forman montículos.
El reflejo del vidrio no compite con la arena. No utilizar anillos de neón,
destellos, desaparición por sacudida o partículas fuera del cristal.

| Token | Valor | Uso |
|---|---|---|
| `HomeBackground` | `#000000` | Fondo y espacio libre |
| `HourglassGold` | `#FFD279` | Granos y acción principal |
| `HourglassAmber` | `#DB922C` | Variación cálida de los granos |
| `HourglassCore` | `#FFF5D5` | Núcleos claros |
| `HourglassGlass` | `#596676` | Contorno del vidrio |
| `HourglassReflection` | `#BBC8D8` | Reflejo fino |
| `HourglassFrame` | `#9F753C` | Soportes y controles |

Se reutilizan Rajdhani para números/acciones y Poppins para instrucciones.
El dorado aquí identifica un material; su restricción a recompensas sigue
aplicando al HUD de Productividad y Asistente.

## Composición

- Panel fijo 720×1280; reloj centrado en un canvas 480×1040 a (120,60).
- El cronómetro Rajdhani de 28 px aparece debajo, centrado a y=1170. Usa
  `MM:SS` o `HH:MM:SS`; Pausado y Finalizado se indican en la misma línea.
- La vista inmersiva no muestra barras de estado, título de módulo ni botones.
- Un toque muestra un panel inferior con controles de 56 px o mayores.
  Los controles se ocultan tras cuatro segundos en marcha; pausa y error
  permanecen accesibles hasta que el usuario los oculta.
- La configuración contiene siete presets de 60 px de altura, una rueda de
  1–120 minutos con cinco filas y acciones Iniciar/Cancelar. No necesita teclado.
- El icono de Inicio tiene el mismo tamaño que los otros módulos, usa un
  pictograma oscuro de reloj y fondo dorado. Su zona táctil admite el nombre.

## Movimiento y estados

Inclinar mueve las superficies y el chorro; invertir cambia la dirección real
del flujo. El cristal y el texto nunca giran. La superficie usa gravedad
filtrada para evitar vibraciones del IMU, sin compensar automáticamente su eje.
Pausar inmoviliza la animación y el tiempo. El tiempo no se reinicia durante una
inversión activa. Después de finalizar, una inversión estable permite otra vuelta.

La tarea del motor y el compositor publican estados acotados. Se dibuja en
canvas dobles en PSRAM y se conserva un único propietario de pantalla: LVGL.
Objetivo: al menos 30 FPS y respuesta táctil inferior a 100 ms, medidos en la
Tab5. Los errores se explican dentro de los controles; no se disfrazan con
gravedad artificial o tiempos inventados.
