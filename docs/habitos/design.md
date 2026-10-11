# Design system — Hábitos

## Identidad

Papel marfil, tinta definida y color útil: minimalismo de tinta electrónica,
sin simular sus limitaciones de refresco. Es una excepción deliberada al HUD
oscuro de Productividad/Asistente y al Inicio con partículas.

| Token | Color | Función |
|---|---|---|
| HabitPaper | #F5F1E8 | Fondo |
| HabitSheet | #FFFCF5 | Formularios y tarjetas |
| HabitInk | #172020 | Texto y contornos de 2 px |
| HabitGreen | #287A59 | Cumplimiento diario |
| HabitBlue | #245FA8 | Acciones, periodos y borde de hoy |
| HabitMuted | #58615C | Texto secundario legible |

Los tokens están en `main/theme.hpp`. Texto blanco sobre verde/azul; tinta
oscura sobre papel. No hay neón, degradados, sombras difusas ni partículas.
Radios de 2–3 px, bordes definidos, espaciado de 12–24 px.

## Tipografía y escala

Rajdhani 40 para títulos/avance, 64 para el contador, 20/28 para controles y
estados; Poppins 16 para descripciones y ayuda. Se reutilizan las fuentes con
tildes, ñ y ü. Los botones y las 84 casillas miden al menos 48 px.

## Composición

- Portada: **Hoy y esta semana**, filtros y Crear. Tarjetas con progreso,
  siete días táctiles y estado textual, sin castigar días libres.
- Horizontal 1280×720: lista a la izquierda y detalle desplazable a la derecha.
- Vertical 720×1280: lista y detalle en vistas distintas; Volver conserva la selección.
- Mosaico de doce columnas/semanas y siete filas/días; 48×48 px con separación
  de 2 px. Cada columna muestra fecha y cumplimiento/meta histórica.
- `X` para cumplido, vacío para sin registro, borde azul para hoy, `-` fuera del
  periodo. La primera semana parcial lleva `*`, sin alterar la meta.
- Continuo: contador protagonista; estado En curso/Detenido, mejor periodo e
  historial secundario. La cuenta congelada no parece una cuenta activa.
- Formularios: área superior desplazable, acciones justo sobre el teclado
  inferior español. Sin MIC. La orientación se bloquea durante la edición.
- Avisos: banner claro de ocho segundos, cuatro acciones y lista de pendientes.
  El indicador compacto se integra en la barra sin esconder energía ni Pausa.

Los segundos cambian el texto existente; no reconstruyen la pantalla. No hay
celebraciones de XP, alarmas de hitos ni rachas perfectas. El diseño comunica
avance flexible y continuidad, no culpa.
