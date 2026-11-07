# Instrucción de implementación para el agente

Diseña e implementa un firmware voxel 3D jugable para M5Stack Tab5 siguiendo
`docs/voxel/plan.md`, que contiene las decisiones técnicas y criterios de prueba.
El objetivo es un modo creativo original inspirado en Minecraft Classic:
explorar, saltar, romper y colocar bloques, completamente sin conexión.

## Contexto y alcance

- Trabaja en un proyecto independiente `voxel_firmware/` dentro del repositorio.
- El firmware existente está en `firmware/` y tiene trabajo en curso. No migres
  sus funciones ni alteres su configuración para construir este proyecto.
- Usa C++17, PlatformIO/Arduino para ESP32-P4, M5Unified y M5GFX, con dependencias
  fijadas. Si hace falta, encapsula APIs nativas de ESP-IDF del framework Arduino
  para la presentación sincronizada. No añadas bibliotecas 3D pesadas.
- Revisa instrucciones locales y estado de git antes de editar. Consulta
  documentación oficial compatible con las versiones realmente seleccionadas.
- El plan no demuestra rendimiento ni compatibilidad de una nueva compilación.
  No conviertas objetivos en resultados sin verificarlos.

## Orden obligatorio

Implementa las fases 0 a 6 del plan en ese orden, conservando un build compilable
al cierre de cada una. Inicia por fase 0: configuración P4, detección de display,
PSRAM y prueba de presentación. Después realiza el diagnóstico de fase 1.
Si no hay dispositivo, completa lo que pueda verificarse en host/build y registra
la validación física pendiente; no declares cumplida la condición de avance de
hardware ni el objetivo de FPS. Puedes preparar módulos de dominio independientes
que no dependan de una decisión de pantalla todavía bloqueada.

## Requisitos técnicos

1. Mundo fijo 64×64×32 con Z vertical, datos de un byte en PSRAM mediante
   `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. IDs exactos:
   aire 0, tierra 1, césped 2, piedra 3, madera 4, hojas 5, arena 6.
2. Generación determinista con Perlin 2D entero, capas y árboles acotados. Límites
   comprobados, semilla configurable y spawn con espacio para cuerpo y cabeza.
3. Render DDA 3D como único motor inicial. RGB565 a 160×90, 16:9 y escalado ×8.
   Perfil opcional 256×144 y ×5 tras mediciones. Distancia inicial 24 bloques.
4. Primer sólido, paleta por cara, cielo y retícula. DDA robusto ante componentes
   cero, coordenadas límite y empates. Sin trigonometría ni normalización por
   píxel; no interpretar el parámetro de un rayo sin normalizar como metros.
5. Cámara float, yaw/pitch ±85°, FOV horizontal 75°. Joystick izquierdo, arrastre
   derecho y botones independientes; asignación estable de contactos táctiles.
6. Física fija 60 Hz, acumulador acotado, AABB y desplazamientos subdivididos.
   Gravedad, salto, techo, paredes y suelo. Movimiento diagonal normalizado.
7. Selección con rayo normalizado de hasta 5 bloques. Romper/colocar por flanco.
   Colocar exige celda válida vacía y no intersectar el jugador. Sin remallado:
   editar el array modifica el resultado del siguiente render.
8. Sin asignaciones del juego por frame, sin crecimiento de contenedores,
   sin crear tareas por frame. Buffers con propiedad explícita; errores de
   inicialización/reserva visibles y controlados.
9. Diferencia doble imagen interna de doble framebuffer nativo. No escribas en
   el buffer que escanea DSI. Comprueba callbacks, caché, alineación, formato y
   orientación con el driver instalado. Dos sprites no prueban ausencia de tearing.
10. No inicialices dos veces el display ni enfrentes dos propietarios del I2C.
    Evalúa nearest-neighbor CPU y después PPA; mide ambas rutas si se implementan.
11. Organiza en `World.h/.cpp`, `Renderer.h/.cpp`, `Player.h/.cpp`, `Input.h/.cpp`
    y los módulos auxiliares definidos en el plan. Núcleo independiente del HAL.
12. Mantén el alcance del MVP. Guardado, audio, supervivencia y multijugador
    quedan fuera; informa que el mundo editado se reinicia al apagar.

## Verificación y comunicación

- Ejecuta el build real de PlatformIO y registra comando, resultado y versiones.
- Prueba en host límites, DDA, selección, colisiones y colocación. Usa sanitizers
  si están disponibles. No sustituyas pruebas físicas con mocks.
- Instrumenta tiempo de render/presentación/física, FPS de imágenes nuevas
  presentadas, pasos DDA y mínimos de heap/PSRAM. Evita logs por píxel o por tacto.
- En hardware aplica las escenas reproducibles y sesión de estabilidad del plan.
  Mínimo aceptable: 20 FPS; meta: 30. No anuncies rendimiento garantizado.
- Cuando una fase no pase, identifica el bloqueo con evidencia y resuelve lo
  posible dentro de su alcance. No la marques como completada por tener código.
- Informa qué se implementó, qué se compiló, qué se probó físicamente y qué queda
  pendiente. Documenta hallazgos en `docs/voxel/`.
- No flashees como parte de una tarea solo de planificación. Cuando se solicite
  instalar, usa los archivos y offsets reales del build y comprueba el puerto.

La primera acción de implementación es obtener una pantalla diagnóstica autónoma
con memoria, contactos táctiles y presentación medidos; el núcleo voxel viene
después de verificar esa base.
