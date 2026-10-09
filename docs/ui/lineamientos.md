# Lineamientos de UI y calidad visual

Estándar de identidad visual y calidad de interfaz para el firmware de El Sistema
(M5Stack Tab5, pantalla táctil de 5", 1280×720 y 720×1280, ESP32‑P4).

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

  > **Necesidad documentada (iteración 2).** Se introdujeron Rajdhani SemiBold y
  > Poppins Regular porque este mismo apartado pide una fuente
  > geométrica/futurista para los elementos del Sistema y texto corrido legible
  > para el contenido, y Montserrat cubría ambos roles con una sola voz.
  > Procedencia, cobertura, licencia y regla de asignación en
  > `firmware/main/fonts/README.md`.

- Preservar comportamientos, flujos y datos existentes: este documento cambia
  cómo se ve y se siente la app, no qué hace.

## 3. Filosofía de diseño

El Sistema usa contextos visuales intencionales. **Inicio** es el espacio de
entrada: negro, lúdico, táctil y con partículas de luz. **Productividad y
Asistente** conservan el HUD oscuro de fantasía/RPG que ya tienen. No se debe
forzar una paleta única entre Inicio y los módulos: la coherencia se mantiene en
tipografía, escala táctil, claridad, ritmo y calidad de ejecución.

**Reloj de arena** añade un contexto inmersivo negro, con cristal y arena de
luz dorada; sus controles usan los componentes compartidos. El dorado representa
material en ese módulo, no XP. Su sesión oculta la barra de estado y bloquea
el panel vertical para que la rotación física cambie únicamente la arena.

**Radar** usa un instrumento circular verde sobre negro, con señales Wi-Fi
amarillas y BLE cian. Estos colores identifican tipos de radio, no XP. Su
barrido es una excepción decorativa explícita: nunca inventa mediciones y
siempre muestra `INTENSIDAD · NO UBICACIÓN`. Comparte tipografía, controles
táctiles y navegación; admite las cuatro orientaciones de la interfaz.

**Tetris** tiene identidad arcade de 8 bits: fondo negro, bloques de siete
colores y tipografía pixelada original. Juega en vertical fija, oculta la barra
global y vuelve a Inicio desde pausa. Sus colores no representan XP ni rangos.
Guía propia en `docs/tetris/design.md`.

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
- **Consistencia por contexto.** Un único diseño por concepto dentro de cada
  contexto; Inicio y los módulos pueden tener identidades distintas.

## 4. Identidad visual

### Paletas

#### Inicio

| Uso | Valor |
|---|---|
| Fondo negro puro | `#000000` |
| Superficie auxiliar | `#10161F` |
| Texto principal | `#F0F7FF` |
| Texto secundario | `#A7B4C4` |
| Pictogramas de iconos claros | `#17202B` |
| Acción azul cielo | `#6FC2FF` |
| Acento amarillo | `#FFDE00` |
| Partículas de luz | centro `#FFFDF8`; tintes azul `#B6ECFF`, cálido `#FFE6AD`, rosa `#FFD0E8` y lavanda `#D9D0FF` |

Usar contornos oscuros de 2 px, esquinas casi cuadradas y sombra corta, dura y
desplazada. Los módulos se representan con iconos compactos tipo aplicación de
teléfono y nombre debajo, no con tarjetas grandes. El fondo de partículas cubre
toda la pantalla por detrás de los controles, con perfiles radiales suaves y
núcleos casi blancos de tamaño reducido. Los halos superpuestos suman luz.
El azul identifica Productividad; el amarillo
distingue Asistente. Texto claro sobre placas negras protege la legibilidad.
No reutilizar la mascota, ilustraciones, logotipo ni recursos de MotherDuck.

#### Módulos: Productividad y Asistente

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

En el HUD oscuro, el cian es acento: **nunca más de ~10 % del área de pantalla**.
En Inicio, el azul y el amarillo se limitan a acciones, índices y pequeñas
marcas; domina el fondo negro. Los tintes multicolor se limitan a las partículas,
no al relleno de superficies ni al texto corrido.

### Tipografía

- Fuente geométrica/futurista para elementos del Sistema (títulos, números,
  niveles, encabezados); texto corrido en fuente legible de buen peso.
- Jerarquía mediante tamaño, peso y luminosidad — nunca solo color.
- Cuerpo mínimo ~16 px. Elementos táctiles mínimo ~48 px de alto.

### Forma y materiales

- **Ventanas del Sistema (módulos oscuros):** paneles con marco cian fino (1–2 px)
  y brillo sutil simulado (borde interior más claro que el exterior).
- **Inicio:** iconos compactos de borde oscuro firme de 2 px, radio de 8 px y
  sombra corta y dura desplazada, sobre negro con partículas de luz.
- Los módulos oscuros conservan esquinas moderadamente redondeadas (8–12 px) y
  esquinas cortadas tipo HUD solo en elementos de máximo foco.
- Scanlines/glitch: solo como micro‑detalle estático de fondo o transición
  puntual. Nunca persistentes ni animados en reposo.

## 5. Componentes del Sistema (requisitos visuales)

- **Ventana del Sistema (módulos):** diálogos y notificaciones con marco
  característico y encabezado tipo `[ SISTEMA ]` / `⚠ ALERTA`. Un solo estilo
  dentro de los módulos.
- **Iconos de Inicio:** pictograma simple de 58×58 px con nombre debajo y zona
  táctil amplia de 152×110 px; no son paneles HUD ni tarjetas descriptivas.
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
- Interpolar propiedades baratas: opacidad, posición y escala. La transición
  de orientación de Inicio es la excepción acotada: giro de los controles
  durante 320 ms, no del canvas de fondo ni de toda la pantalla. Sin blur en
  tiempo real ni blend modes costosos.
- Direcciones coherentes: modales desde el centro con escala sutil; listas desde
  desplazamiento.
- **Completar una misión:** celebración contenida (0,5–1 s: pulso, +XP flotante,
  brillo en la barra) que nunca bloquee la interacción.
- La estabilidad del framerate es parte de la calidad. La física de partículas
  de Inicio funciona solo mientras Inicio está visible, como fondo completo y
  con objetivo de 50 fps (validar 30–60 fps en el dispositivo). Se compone en
  canvas dobles en PSRAM, detrás de controles y textos con contraste protegido;
  no puede competir con el tacto. En el resto de la interfaz se prioriza fluidez.

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
- Dos versiones del mismo componente dentro de un contexto visual.
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
- [ ] En módulos, cian ≈ acento (< ~10 %); en Inicio predomina negro y el neón se limita a partículas
- [ ] Texto siempre legible y contrastado
- [ ] Deuda visual detectada fuera de alcance: registrada

## 10. Criterio final

Ante cualquier decisión de UI no cubierta aquí, responder: **"¿Esto hace la misión
del usuario más clara, más satisfactoria o más legible en una pantalla táctil de 5
pulgadas?"**. Si no, eliminarlo o simplificarlo.
