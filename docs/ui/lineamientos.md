# Lineamientos de UI y calidad visual

Estándar de identidad visual y calidad de interfaz para el firmware de El Sistema
(M5Stack Tab5, pantalla táctil de 5", 1280×720, ESP32‑P4).

Este documento **no** define funcionalidad, arquitectura ni features. Define cómo
se ve y se siente la app, y el criterio de decisión para todo trabajo de interfaz.

---

## 1. Contexto

Aplicación que registra tareas como "misiones" con progresión de nivel y rango
estilo RPG. Ya existen código, pantallas y componentes implementados.

## 2. Modo de aplicación sobre código existente

- No reescribir UI funcional por gusto estético. Los cambios visuales son
  graduales y justificados por estos lineamientos.
- **Regla de toque:** toda pantalla, pantalla parcial o componente de UI que se
  modifique debe quedar en cumplimiento con este documento antes de dar por
  terminada la tarea. No dejar UI "a medio migrar".
- Si se descubren inconsistencias fuera del alcance de la tarea actual:
  corregirlas si son triviales; si no, registrarlas como **deuda visual** en una
  lista visible para iteraciones futuras (`docs/ui/deuda-visual.md`).
- Los componentes nuevos nacen en compliance. No crear variantes nuevas de
  componentes que ya existen; extender y normalizar los existentes.
- Reutilizar assets ya embebidos (fuentes, imágenes, estilos). No introducir
  fuentes nuevas salvo necesidad real documentada.
- Preservar comportamientos, flujos y datos existentes: este documento cambia
  cómo se ve y se siente la app, no qué hace.

## 3. Filosofía de diseño

"El Sistema, refinado": la estética del sistema de nivelación de Solo Leveling
(ventanas holográficas cian, jerarquía RPG, notificaciones del Sistema) ejecutada
con la disciplina del diseño de Apple.

- **Claridad antes que decoración.** El neón y el estilo HUD son acento, nunca
  ruido que degrade la legibilidad.
- **Deferencia al contenido.** La UI se retira para que las misiones y el progreso
  del usuario dominen la pantalla.
- **Profundidad por capas, no por blur.** Jerarquía espacial mediante luminosidad
  de superficies y bordes luminosos, no sombras difusas.
- **Movimiento con propósito.** Cada animación comunica cambio, guía la atención o
  da feedback. Nada se mueve "por verse bien".
- **Feedback inmediato.** Toda interacción produce respuesta visual en <100 ms,
  aunque la acción real tome más tiempo.
- **Consistencia total.** Un solo diseño por concepto en toda la app.

## 4. Identidad visual

### Paleta

| Uso | Valor |
|---|---|
| Fondo base | negro azulado profundo (`#050A14` – `#0A1220`) |
| Superficies | azul oscuro en niveles crecientes (`#0F1A2C`, `#16243D`) — la elevación se expresa con brillo, no sombras |
| Acento primario "Sistema" | cian brillante (`#00E5FF`) |
| Acento secundario (misiones especiales / estados raros) | violeta‑azul (`#7C6CFF`) |
| Peligro / fallo | rojo neón (`#FF3B5C`) |
| Éxito / recompensa | cian brillante; dorado puntual solo en logros mayores |
| Texto principal | `#E8F2FF` |
| Texto secundario | `#7A8CA3` |

El cian es acento: **nunca más de ~10 % del área de pantalla**.

### Tipografía

- Fuente geométrica/futurista para elementos del Sistema (títulos, números,
  niveles, encabezados); texto corrido en fuente legible de buen peso.
- Jerarquía mediante tamaño, peso y luminosidad — nunca solo color.
- Cuerpo mínimo ~16 px. Elementos táctiles mínimo ~48 px de alto.

### Forma y materiales

- **Ventanas del Sistema:** paneles con marco cian fino (1–2 px) y brillo sutil
  simulado (borde interior más claro que el exterior).
- Esquinas redondeadas moderadas (8–12 px); esquinas cortadas tipo HUD solo en
  elementos de máximo foco (modales, nivel, rango).
- Scanlines/glitch: solo como micro‑detalle estático de fondo o transición
  puntual. Nunca persistentes ni animados en reposo.

## 5. Componentes del Sistema (requisitos visuales)

- **Ventana del Sistema:** diálogos y notificaciones con marco característico y
  encabezado tipo `[ SISTEMA ]` / `⚠ ALERTA`. Un solo estilo, idéntico en toda la
  app.
- **Barras de progreso (XP, nivel):** delgadas, con pulso de luz al llenarse.
  Feedback visible al ganar XP.
- **Elementos RPG (nivel, rango E–S, stats):** misma jerarquía tipográfica que el
  resto de la app; integrados al layout, no sobrepuestos.
- **Lista de misiones:** filas espaciosas con separadores tenues. Dificultad y
  estado codificados con color + icono + etiqueta de texto (nunca solo color).
- **Estados completos:** vacío, carga, error y éxito tienen diseño propio
  (p. ej. vacío: "No hay misiones activas — el Sistema te espera").

## 6. Movimiento y animación

- Duración estándar 200–350 ms con curvas ease‑out o springs cortos. Nada lineal,
  nada brusco, nada largo.
- Interpolar **solo** propiedades baratas: opacidad, posición, escala. Sin blur en
  tiempo real ni blend modes costosos.
- Direcciones coherentes: modales desde el centro con escala sutil; listas desde
  desplazamiento.
- **Completar una misión:** celebración contenida (0,5–1 s: pulso, +XP flotante,
  brillo en la barra) que nunca bloquee la interacción.
- La estabilidad del framerate es parte de la calidad: si una animación compromete
  los 60 fps, simplificarla. Un 60 fps estable vale más que cualquier efecto.

## 7. Interacción y feedback

- **Estado presionado:** aumento sutil de luminosidad/escala del elemento.
- Avisos y confirmaciones vía notificaciones del Sistema: discretos, breves, con
  animación de entrada/salida consistente.
- Cada evento clave (misión aceptada, completada, subida de nivel) tiene su firma
  visual reconocible y repetible.

## 8. Antipatrones (prohibido)

- Neón saturado en grandes áreas o degradando el contraste del texto.
- Glitch/ruido constante mientras el usuario lee o navega.
- Animaciones largas que bloqueen o interrumpan la interacción.
- Elementos decorativos que no comuniquen jerarquía, estado o física.
- Dos versiones del mismo componente en distintos flujos.
- Rediseños totales por preferencia: cambios visuales graduales y trazables a
  estos lineamientos.

## 9. Checklist de salida (Definition of Done visual)

Antes de terminar cualquier tarea que toque UI:

- [ ] Lo modificado cumple estos lineamientos por completo
- [ ] Contraste texto/fondo legible en pantalla táctil de 5"
- [ ] Elementos táctiles ≥ 48 px de alto
- [ ] Animaciones solo con propiedades baratas y framerate estable
- [ ] Estados definidos: presionado, vacío, carga, error, éxito
- [ ] Sin variantes nuevas de componentes existentes
- [ ] Cian ≈ acento (< ~10 % de pantalla), texto siempre legible
- [ ] Deuda visual detectada fuera de alcance: registrada

## 10. Criterio final

Ante cualquier decisión de UI no cubierta aquí, responder: **"¿Esto hace la misión
del usuario más clara, más satisfactoria o más legible en una pantalla táctil de 5
pulgadas?"**. Si no, eliminarlo o simplificarlo.
