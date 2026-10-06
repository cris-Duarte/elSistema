# Design system — Productividad

## Propósito

Productividad convierte tareas en misiones y pone el foco visual en una misión
activa, con su rango, tiempo, descripción y acciones. Zona de Enfoque, Backlog e
Historial son superficies auxiliares para organizar y consultar.

## Identidad

Conserva el HUD oscuro de El Sistema: superficies azul-negro, jerarquía RPG,
marcos discretos y cian como acento. No adopta el fondo negro puro ni las
partículas multicolor del lanzador.

| Rol | Token/valor |
|---|---|
| Fondo | `#050A14` |
| Superficie | `#0F1A2C` |
| Superficie elevada | `#16243D` |
| Borde | `#223A5C` |
| Acento Sistema | `#00E5FF` |
| Texto principal | `#E8F2FF` |
| Texto secundario | `#7A8CA3` |
| Rango `S` | `#FFC24B` |

Consultar los tokens de rango `E–A` en `firmware/main/theme.hpp`; no redefinirlos
localmente. Rajdhani se usa para títulos, cronómetros, rangos y acciones; Poppins
para descripción y texto escrito por la persona.

## Jerarquía y composición

- En la pantalla raíz: metadatos del rango, cronómetro de 128 px, título y
  descripción, en ese orden.
- La misión activa domina en horizontal y vertical.
- Sin misión activa, la Zona de Enfoque ocupa el lugar protagonista.
- Zona de Enfoque y Backlog se colapsan desde tabs; el Historial permite búsqueda,
  filtros y detalle.
- La cabecera raíz ofrece retorno a Inicio; formularios y diálogos conservan
  Cancelar/Volver hacia Productividad.

## Interacción y estados

Los controles táctiles miden al menos 48 px. El rango se acompaña de etiqueta,
no solo de color; su peso visual crece de `E` a `S`. Pausar, alternar y completar
conservan el estado y ofrecen feedback inmediato. El teclado en español siempre
está disponible como alternativa al dictado.

## Límites

- No llevar las partículas neón de Inicio a la misión activa.
- No usar cian como gran relleno decorativo.
- Las animaciones deben ser breves, legibles y no bloquear la interacción.

## Fuente común

Las reglas compartidas están en
[`docs/ui/lineamientos.md`](../ui/lineamientos.md).
