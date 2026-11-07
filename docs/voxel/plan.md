# Tab5 Voxel — plan del nuevo firmware

Fecha: 7 de noviembre de 2026. Este documento conserva el diseño inicial.
La implementación y su validación actual se describen en [estado.md](estado.md).

## 1. Objetivo y alcance

Crear un juego voxel 3D original inspirado en Minecraft Classic, para un jugador,
sin conexión, con exploración, gravedad, salto, destrucción y colocación de siete
tipos de bloques contando aire. Orientación horizontal fija y controles táctiles.
El primer entregable jugable será un modo creativo acotado, sin supervivencia.

Se propone un proyecto independiente `voxel_firmware/` dentro de este repositorio.
El firmware actual está en `firmware/` y tiene modificaciones en curso. El nuevo
proyecto tendrá configuración, dependencias, particiones y directorio de build
propios. La entrega inicial fue de planificación; la implementación posterior
se registra por separado en `estado.md`.

Fuera del MVP: red, multijugador, enemigos, crafting, líquidos, transparencia,
iluminación dinámica, audio, mundo infinito y guardado de partidas. Sin guardado,
los cambios del mundo se pierden al reiniciar; debe indicarlo el menú. Guardado
versionado y texturas originales pequeñas quedan para una versión posterior.

## 2. Hardware y base de desarrollo

La documentación oficial identifica ESP32-P4 de dos núcleos a 360 MHz, 32 MB de
PSRAM, 16 MB de flash y pantalla IPS MIPI-DSI de 1280×720. Existen revisiones del
display/táctil ILI9881C+GT911, ST7123 y ST7121. No asumir una revisión concreta ni
una matriz de botones de juego: el control integrado documentado es de
encendido/arranque. Todos los controles del MVP deben funcionar por tacto. [S1]

**Ruta principal:** C++17, PlatformIO con Arduino para ESP32-P4, M5Unified y
M5GFX. Las llamadas nativas de ESP-IDF que ya incorpora Arduino se encapsularán
en el adaptador de pantalla si son necesarias. No depender de una instalación
global de Arduino IDE ni del entorno del firmware existente.

La guía oficial publica como punto de partida la plataforma pioarduino
`54.03.21` y la placa `esp32-p4-evboard`. Es una configuración candidata, no una
compilación validada aquí. La fase 0 debe compilarla, comprobar la configuración
de flash/PSRAM y fijar versiones o commits exactos de todas las dependencias. [S1]

Referencias locales ya presentes:

- `firmware/main/idf_component.yml`: M5Unified 0.2.25 y M5GFX 0.2.32 fijados;
  son candidatos iniciales, no prueba de compatibilidad con el nuevo build Arduino.
- `firmware/main/m5_platform.cpp`: evita inicializar dos propietarios de display
  y serializa el I2C compartido. El firmware actual usa BSP+LVGL, por lo que no
  debe copiarse como si fuese una inicialización autónoma de M5Unified.
- `firmware/managed_components/m5stack__m5gfx/src/lgfx/v1/platforms/esp32p4/Panel_DSI.inl`:
  la copia local configura dos framebuffers pero obtiene uno para su dibujo.
  Esto no demuestra que una operación gráfica ordinaria haga intercambio
  sincronizado de buffers.

M5Stack pide M5Unified >=0.2.23 y M5GFX >=0.2.30 para compatibilidad del reset
con las revisiones actuales. Si se actualizan versiones, revisar requisitos
mutuos y repetir el diagnóstico de pantalla. No usar dependencias flotantes. [S1, S2]

## 3. Decisiones del motor

| Área | Decisión para el MVP | Motivo |
| --- | --- | --- |
| Render | DDA 3D, primer voxel sólido por píxel | Evita geometría y remallado; ofrece un recorrido común para selección |
| Mundo | 64×64×32, Z vertical, 1 byte por voxel | Capacidad y accesos acotados |
| Chunks | 4×4 regiones lógicas de 16×16×32 | Organización y futuras revisiones; no duplican los voxels |
| Imagen inicial | RGB565, 160×90 | 16:9 y escalado entero ×8 |
| Perfil de calidad | RGB565, 256×144 | 16:9 y escalado entero ×5; se habilita tras medir |
| Distancia visual | 24 bloques, configurable entre 12 y 32 | Limita trabajo y permite niebla simple |
| Interacción | Rayo central normalizado, máximo 5 bloques | Resultado independiente de la resolución |
| Física | Paso fijo de 1/60 s | Comportamiento independiente de los FPS |
| Paralelismo | Una tarea de juego inicialmente | Evita carreras entre edición, render y física |

DDA no genera caras internas ni necesita mallas: termina al alcanzar el primer
bloque opaco. La fase de descarte de caras del prompt se sustituye por terminación
temprana, límites de recorrido y optimización de accesos. Todos los bloques,
incluidas hojas, serán sólidos y opacos en esta versión.

Un rasterizador con mallas queda como alternativa si las mediciones muestran que
DDA no alcanza el mínimo a 160×90 y distancia 12. Requeriría clipping contra near
plane, culling, buffers de capacidad fija, remallado con presupuesto por frame y
Z-buffer. Ordenar chunks por distancia con Painter no resuelve por sí solo toda
la oclusión. No desarrollar ambos motores a la vez.

## 4. Módulos y contratos

Estructura propuesta; estos archivos todavía no existen:

| Archivo bajo `voxel_firmware/` | Responsabilidad |
| --- | --- |
| `platformio.ini` | Placa, plataforma y librerías fijadas, perfiles de build |
| `include/Config.h` | Dimensiones, límites, resolución, FOV y presupuesto |
| `include/Types.h` | Vectores, bloques, entradas, hits y estadísticas POD |
| `include/World.h`, `src/World.cpp` | Reserva PSRAM, acceso seguro y chunks lógicos |
| `include/Terrain.h`, `src/Terrain.cpp` | Ruido entero, capas y árboles por semilla |
| `include/Camera.h`, `src/Camera.cpp` | Base de cámara y rayos de perspectiva |
| `include/Raycast.h`, `src/Raycast.cpp` | DDA compartido por imagen e interacción |
| `include/Renderer.h`, `src/Renderer.cpp` | Imagen RGB565, paleta y sombreado de caras |
| `include/DisplayBackend.h`, `src/DisplayBackend.cpp` | Escalado, orientación y sincronización de presentación |
| `include/Player.h`, `src/Player.cpp` | AABB, gravedad, salto y spawn |
| `include/Input.h`, `src/Input.cpp` | Multitáctil y traducción a intenciones |
| `include/Game.h`, `src/Game.cpp` | Orden de actualización y acciones del mundo |
| `include/Hud.h`, `src/Hud.cpp` | Retícula, selección, controles y diagnóstico |
| `include/Diagnostics.h`, `src/Diagnostics.cpp` | Tiempos, memoria y contadores sin reservar por frame |
| `src/main.cpp` | Inicialización y bucle Arduino |
| `test/` | Pruebas del núcleo en host, sin drivers |

Contratos principales:

- `World::init()` reserva una vez, informa fallo y libera de forma controlada.
  `get/set` comprueban coordenadas antes de convertir a índice. `set` devuelve
  éxito/fallo. El render puede usar un acceso interno rápido solo tras validar
  la celda en DDA. No exponer escritura arbitraria del array.
- `RaycastHit` contiene validez, voxel alcanzado, normal entera de la cara de
  entrada, parámetro de rayo y material. La selección usa distancia en bloques.
- `InputState` contiene movimiento, delta de cámara, salto y acciones por flanco.
  El dominio no conoce coordenadas de pantalla ni APIs de M5Unified.
- `Renderer` recibe mundo y cámara de solo lectura y un destino de dimensiones
  explícitas. `DisplayBackend` conserva la propiedad temporal de cada buffer.
- `Player` y el núcleo matemático no incluyen cabeceras de Arduino; la reserva
  del mundo se adapta en host para poder probarlos.

## 5. Mundo y generación

Coordenadas: X/Y horizontales, Z vertical. Índice plano:
`x + 64 * (y + 64 * z)`. Usar tipos de índice suficientemente anchos y
`static_assert` para tamaño y codificación. No asumir que un resultado negativo
convertido a unsigned siga siendo una coordenada válida.

| ID | Bloque |
| --- | --- |
| 0 | Aire |
| 1 | Tierra |
| 2 | Césped |
| 3 | Piedra |
| 4 | Madera |
| 5 | Hojas |
| 6 | Arena |

Reserva de voxels con
`heap_caps_malloc(64 * 64 * 32, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`.
Sin fallback silencioso a SRAM interna. Si falla, mostrar error de arranque.
La información persistente del mundo, incluidos metadatos de chunks si se usan,
también vive en la arena PSRAM; pequeños índices temporales pueden estar en stack.

Generación con semilla fija/configurable, Perlin 2D de gradientes enteros y
aritmética de punto fijo, dos octavas iniciales. Evitar divisiones y trigonometría
dentro del recorrido de voxels. Construir piedra, capas de tierra y césped;
arena en cotas bajas, sin agua. Reservar altura para árboles y espacio libre.
Árboles con troncos y copas simples, hash por coordenada/semilla y comprobación de
todos sus límites. Generación reproducible y spawn con suelo y hueco para cuerpo
y cabeza. No generar ni regenerar terreno durante el frame.

Fuera del mapa: render termina y dibuja cielo/niebla, interacción rechaza cambios,
física impone paredes y suelo de límite. No tratar aire exterior como permiso
para indexar fuera del array. La capa inferior no se rompe para mantener suelo.

## 6. Cámara, DDA y apariencia

- Posición `float`; yaw/pitch, pitch limitado a ±85°. FOV horizontal inicial de
  75°, relación de aspecto 16:9. Calcular base forward/right/up una vez por frame.
- Para cada centro de píxel, formar un rayo en perspectiva con la base de cámara.
  Precalcular coordenadas de pantalla y avanzar incrementalmente por la fila;
  sin `sin`, `cos`, `tan` ni normalización por píxel en el bucle caliente.
- En render se permiten direcciones sin normalizar: su parámetro no representa
  distancia euclídea. Para distancia visual/niebla, convertir con la longitud
  precalculada del rayo o usar un corte geométrico equivalente documentado.
  El rayo de interacción sí va normalizado para cumplir el límite de 5 bloques.
- DDA maneja componentes cero con infinito, orígenes en caras y empates de cruce
  con una regla determinista y normal de selección estable. Intersectar la caja
  del mundo si procede. Establecer límite de pasos (p. ej. 192) además de distancia.
- Primer sólido devuelve material y eje de entrada. Paleta de siete entradas y
  tres intensidades precalculadas para techo, cara X y cara Y, sin iluminación
  por voxel ni multiplicaciones de color costosas por píxel.
- Cielo plano o gradiente por filas. Niebla por tabla opcional. Retícula y HUD
  se componen en el buffer antes de presentarlo, nunca sobre la imagen escaneada.

## 7. Pantalla: buffers, escalado y propiedad

Hay dos clases diferentes de buffer: imagen interna del juego y framebuffer
nativo que lee el periférico DSI. Duplicar solo la imagen pequeña no garantiza
ausencia de tearing. La documentación de ESP-IDF 5.4.2 expone framebuffers DPI
y callbacks de panel; la implementación debe verificar las garantías concretas
del driver y el momento en que cada buffer vuelve a estar disponible. [S3]

Objetivo de flujo:

1. CPU renderiza en una imagen pequeña libre.
2. Escalado escribe únicamente en el framebuffer nativo libre, con transformación
   a la orientación física del panel si hace falta.
3. Tras finalizar todas las escrituras y la coherencia de caché necesaria, se
   solicita la presentación con la API comprobada para la versión elegida.
4. La notificación de transferencia/refresh apropiada libera el buffer que ya
   no está en uso. No confundir fin de copia con fin de lectura del panel.

Estados explícitos `FREE`, `RENDERING`, `READY`, `PRESENTING`; cuando no haya
destino libre, esperar con límite o descartar una imagen terminada. No acumular
frames ni sobrescribir un buffer en uso. Callbacks ISR solo notifican, sin
dibujar, asignar memoria o procesar I2C. Manejar timeout y error de presentación.

Primero medir nearest-neighbor por CPU con escala entera y escrituras por filas.
Evaluar PPA como segundo backend: el P4 tiene aceleración de escalado/rotación,
pero no debe confundirse con un motor de vóxeles. Su rendimiento depende del
tráfico PSRAM y exige buffers de entrada/salida distintos para SRM. [S4]
Verificar filtro, alineación, stride, orientación y coherencia de caché con el
SDK instalado; no asumir que el escalado PPA conserva bordes pixelados.

La fase 0 debe comprobar si M5GFX ofrece acceso suficiente para esta propiedad
y sincronización. Si no, usar un backend nativo `esp_lcd` encapsulado y una sola
inicialización del panel, con M5Unified configurado para no crear otro display.
Adaptar la detección a la revisión física. Esa alternativa debe compilar y medirse
antes de construir el juego; no basta con escribir llamadas DMA hipotéticas.

## 8. Presupuesto de memoria y tiempo

| Recurso | Bytes de datos | Ubicación prevista |
| --- | ---: | --- |
| Mundo 64×64×32 | 131 072 | PSRAM |
| Dos imágenes RGB565 160×90 | 57 600 | PSRAM, reserva al inicio |
| Dos imágenes RGB565 256×144, en su lugar | 147 456 | PSRAM, reserva al inicio |
| Dos framebuffers RGB565 1280×720 | 3 686 400 | Gestionados por el driver en PSRAM |
| Subtotal perfil inicial | 3 875 072 | ≈3,70 MiB |
| Subtotal perfil de calidad | 3 964 928 | ≈3,78 MiB |
| Arena adicional del mundo/metadatos | Hasta 16 KiB | PSRAM |
| Tablas de cámara/paleta y estado del motor | Objetivo ≤32 KiB | SRAM interna cuando convenga |

Los subtotales no incluyen SDK, stacks, cachés, descriptores, alineación ni
buffers auxiliares de drivers. No reservar otra pareja nativa si ya la administra
el panel. Objetivo provisional: recursos propios del juego + pantalla <6 MiB y
al menos 8 MiB libres de PSRAM tras iniciar; confirmar consumo real en fase 1.
Registrar también heap interno mínimo y mayor bloque libre, sin imponer un
umbral SRAM ficticio antes de conocer el baseline del driver.

No usar `new`, `malloc`, `String`, crecimiento de contenedores o creación de
tareas por frame en el código del juego. Reservar una sola vez todas las
capacidades de ambos perfiles que realmente se habiliten; no redimensionar
buffers durante una partida. Fallos de reserva tienen salida visible y segura.

A 160×90 hay 14 400 rayos por imagen. El peor recorrido depende de la escena,
la distancia y la dirección; medir pasos medios y máximos, especialmente al
mirar horizontalmente en zonas vacías. No deducir FPS a partir de la CPU.

Escribir una imagen nativa RGB565 completa por frame mueve 1 843 200 bytes:
55,3 MB/s de escrituras a 30 FPS, sin contar lecturas, caché ni scanout. Esto
explica por qué reducir resolución interna no elimina el coste de presentación.

| Métrica | Criterio propuesto para el MVP |
| --- | --- |
| FPS presentados | ≥20 en cada ventana de 5 s de las escenas acordadas |
| Meta preferida | 30 FPS sostenidos si el hardware medido lo permite |
| Tiempo entre frames presentados, p95 | ≤50 ms, fuera de arranque/pausa |
| Latencia tacto → imagen, p95 | Objetivo ≤100 ms; medir por cámara si es necesario |
| Estabilidad | 30 min de juego sin reset, watchdog ni caída progresiva del heap |
| Integridad visual | Sin corrupción ni tearing visible en patrón de prueba |

Para 20 FPS, presupuesto inicial orientativo de 50 ms: render 30, presentación
12, física/entrada 3 y margen 5. No es un resultado obtenido. Si se solapan
operaciones, registrar duración de etapas y latencia total separadamente.
Mantener la física a 60 Hz con acumulador, máximo cuatro pasos por iteración y
recorte explícito del exceso tras pausas; evitar espiral de recuperación.
No fijar tareas a núcleos ni paralelizar hasta contar con perfil. Si se añade un
worker, compartir snapshots de cámara/entrada y aplicar ediciones entre renders.

## 9. Física y controles

Posición del jugador en sus pies, AABB de ancho 0,6 y alto 1,8 bloques; ojos a
1,62. Velocidad horizontal inicial 3,5 bloques/s, gravedad -18 bloques/s²,
salto +6 bloques/s y caída limitada a -20 bloques/s; son parámetros de ajuste.
Normalizar diagonales. Resolver X, Y y Z por separado con epsilon pequeño;
subdividir desplazamientos para que ningún paso exceda 0,25 bloques. Probar
paredes, esquinas, techo y suelo. `grounded` solo procede del contacto inferior;
el salto requiere contacto y flanco de pulsación.

Controles propuestos en coordenadas lógicas 1280×720:

| Zona | Acción |
| --- | --- |
| Inferior izquierda | Joystick virtual, zona muerta y movimiento relativo al yaw |
| Derecha, fuera de botones | Arrastre de mirada; sensibilidad independiente de FPS |
| Inferior derecha | Botones separados: saltar, romper y colocar |
| Franja inferior central | Seis slots sólidos; selección por toque |
| Esquina superior | Pausa, diagnóstico y regreso al menú |

Cada contacto conserva su función hasta soltarse, incluso al cruzar de zona.
Comprobar en fase 1 IDs estables o asociación temporal por zona con la API real.
Probar caminar + mirar + acción simultáneos. Si el panel/API limita contactos,
ajustar la distribución con medición; no ocultar la limitación. Transformar
coordenadas táctiles con la misma orientación lógica que la imagen.
Perder contactos o pausar anula movimiento y acciones retenidas.

Romper/colocar actúan por flanco, con cooldown inicial de 150 ms. Al colocar,
usar `hit.voxel + hit.normal`, exigir aire, coordenadas válidas y ausencia de
intersección con el jugador. El mundo se edita antes de renderizar; DDA refleja
el cambio sin remallado. Una pulsación de slot no debe minar al mismo tiempo.

## 10. Fases y condiciones de avance

Cada fase conserva un build compilable. Al cerrar una fase, registrar comandos,
versiones, pruebas y resultados reales. Una prueba host no valida el hardware.

| Fase | Implementación | Validación para avanzar |
| --- | --- | --- |
| 0 · Viabilidad | Proyecto PlatformIO autónomo; dependencias fijadas; elegir backend de pantalla | Build P4 desde cero; identificar revisión física; prueba de swap/refresh sin doble inicialización |
| 1 · Hardware | RGB565, dos buffers, escalado, tacto y diagnóstico | 10 min de patrones en movimiento; sin corrupción; medir coste de presentación y contactos simultáneos |
| 2 · Mundo | Arena PSRAM, 7 IDs, Perlin entero, capas, arena y árboles | 128 KiB de voxels; reproducibilidad; pruebas de límites, reservas fallidas y spawn |
| 3 · Cámara | Base de perspectiva, rayos y movimiento libre solo de diagnóstico | Orientación/FOV correctos; pitch limitado; input por tiempo; sin distorsión de aspecto |
| 4 · Render | DDA, colores por cara, cielo, distancia y perfil | Escenas vacía, plana, bosque y construida; ≥20 FPS a 160×90; pruebas DDA y medición de pasos |
| 5 · Física | Paso fijo, AABB, gravedad, salto y spawn seguro | Sin atravesar paredes/techo; diagonal normal; caída rápida y FPS variables reproducibles |
| 6 · Juego | Retícula, selección, romper/colocar, slots y pausa | Alcance 5; colocar no encierra al jugador; editar bordes; controles simultáneos y sesión 30 min |

Pruebas de núcleo mínimas y útiles: indexación/límites del mundo, generación por
semilla, DDA paralelo a ejes y en empates/caras, alcance, normal y celda adyacente,
colisiones a distinta velocidad y rechazo de colocación contra el jugador.
Usar ASan/UBSan en host si el compilador disponible los admite.

Escenas de rendimiento reproducibles: semilla documentada, cámara fija mirando
cielo, horizonte de terreno plano, bosque denso y estructura editada con huecos.
Medir 60 s por escena tras calentamiento; conservar resolución, distancia, FOV,
brillo, alimentación, versión y configuración. FPS significa imágenes completas
presentadas, no iteraciones de `loop()` ni refrescos del panel sin imagen nueva.

Si falla rendimiento: perfilar primero; optimizar accesos y rayos, reducir
distancia hasta 12 y reevaluar. El perfil 256×144 se habilita solamente tras pasar
los criterios. Si el mínimo sigue sin alcanzarse, registrar el bloqueo y evaluar
el rasterizador alternativo antes de declarar el MVP logrado.

## 11. Entregas esperadas

1. Proyecto independiente compilable y README con pasos exactos, versiones y
   requisitos del Tab5, sin depender de archivos generados del proyecto anterior.
2. Núcleo C++ modular con `World`, `Renderer`, `Player`, `Input` y adaptadores.
3. Pruebas host y firmware de diagnóstico; informe de mediciones físicas.
4. Binarios únicamente cuando exista una compilación real identificable.
5. Instrucciones de instalación y recuperación basadas en los offsets/particiones
   generados por ese build. La planificación no requiere flashear el dispositivo.

## 12. Fuentes y límites de esta revisión

- [S1 · M5Stack Tab5: hardware, revisiones y ejemplo PlatformIO](https://docs.m5stack.com/en/core/Tab5)
- [S2 · Tab5 Arduino: compilación e instalación](https://docs.m5stack.com/en/arduino/m5tab5/program)
- [S3 · ESP-IDF 5.4.2: MIPI DSI y framebuffers DPI](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html)
- [S4 · ESP-IDF 5.4.2: PPA](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32p4/api-reference/peripherals/ppa.html)

Se revisaron documentación oficial y archivos locales de configuración, plataforma
y display. No se compiló un firmware nuevo ni se conectó o midió el Tab5. La
elección de DDA, presupuestos, controles y umbrales son decisiones de diseño
sujetas a las pruebas anteriores; no son prestaciones verificadas del equipo.
