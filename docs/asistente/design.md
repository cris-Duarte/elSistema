# Design system — Asistente

## Propósito

El Asistente es una entidad visual de El Sistema para interacciones por toque,
micrófono y altavoz. El personaje y sus estados son la información principal;
la pantalla no debe sugerir que existe IA conversacional antes de conectarla.

## Identidad

Mantiene el HUD oscuro de los módulos: negro azulado, paneles azul oscuro, texto
claro y cian/violeta como acentos. Inicio usa negro puro y partículas neón, pero el
cambio de contexto ocurre al entrar o salir del módulo.

| Rol | Token/valor |
|---|---|
| Fondo | `#050A14` |
| Superficie | `#0F1A2C` |
| Superficie elevada | `#16243D` |
| Borde | `#223A5C` |
| Acento principal | `#00E5FF` |
| Acento del personaje | `#7C6CFF` |
| Texto principal | `#E8F2FF` |
| Texto secundario | `#7A8CA3` |

Rajdhani se usa para estados, nombres y acciones; Poppins para explicaciones. El
personaje conserva sus recursos actuales; no se incorpora la mascota de la
referencia de Inicio.

## Jerarquía e interacción

- El personaje ocupa la mayor parte del escenario. Estados: reposo, saludo,
  atención, escucha, procesamiento, respuesta, celebración y error.
- Micrófono, detener, cancelar, reintentar y retorno a Inicio son controles
  táctiles de al menos 48 px.
- Las etiquetas aclaran qué sucede; el color o el gesto no son la única señal.
- Salir del módulo cancela el turno de voz activo.
- La cabecera raíz ofrece retorno a Inicio; los subflujos mantienen su salida.

## Límites

- No sugerir conversación inteligente hasta implementar un modelo.
- No escuchar en segundo plano ni activar por palabra de vigilia.
- No incluir funciones sociales o actuadores en el módulo visual actual.
- Audio y red no deben bloquear LVGL ni el cronómetro de Productividad.

## Fuente común

Los tokens y reglas compartidas están en
[`docs/ui/lineamientos.md`](../ui/lineamientos.md).
