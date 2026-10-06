# Design system — Inicio

## Propósito

Inicio es el contenedor y lanzador de El Sistema. Permite elegir un módulo y
mostrar de un vistazo la hora, fecha, conectividad y batería. No presenta
misiones activas ni accesos directos a tareas.

## Identidad

Interfaz táctil y lúdica sobre negro puro, con pequeñas partículas de luz,
adaptada a la pantalla de 5 pulgadas del Tab5. Conserva los iconos compactos y
la geometría de la referencia original, sin copiar su marca, mascota o recursos.

| Token | Valor | Uso |
|---|---|---|
| `HomeBackground` | `#000000` | Fondo negro |
| `HomeSurface` | `#10161F` | Superficies auxiliares |
| `HomeInk` | `#F0F7FF` | Texto principal |
| `HomeMuted` | `#A7B4C4` | Texto de apoyo |
| `HomeIconInk` | `#17202B` | Pictogramas y contornos de iconos claros |
| `HomeBlue` | `#6FC2FF` | Productividad y estado conectado |
| `HomeCanary` | `#FFDE00` | Asistente |
| `HomeLightCore` | `#FFFDF8` | Centro casi blanco |
| `HomeParticleCyan` | `#B6ECFF` | Luz azul fría |
| `HomeParticleLime` | `#FFE6AD` | Luz cálida (nombre de token conservado) |
| `HomeParticleRose` | `#FFD0E8` | Luz rosada |
| `HomeParticleLavender` | `#D9D0FF` | Luz lavanda |

Los iconos usan borde oscuro de 2 px, radio de 8 px y sombra dura corta.
Se usan Rajdhani para títulos/cifras y Poppins para texto de apoyo.

## Composición

- Hora local `HH:MM` y fecha `DD/MM/AAAA` en primer plano.
- Wi‑Fi y batería en la barra superior; el Wi-Fi abre conectividad con un área
  táctil de al menos 48 px.
- Iconos de aplicación de 96×96 px, con nombre debajo y zona táctil de
  152×148 px. Productividad usa una lista; Asistente, una cara de robot.
- Rejilla centrada extensible, hasta cuatro módulos por fila. No mostrar las
  antiguas tarjetas grandes, descripciones ni números de módulo.
- Fondo animado continuo de borde a borde, detrás de reloj, estado y módulos,
  tanto en horizontal como en vertical. No usar un contenedor de arena.
- El reloj queda en el tercio superior y los iconos debajo; los textos usan
  pequeñas superficies negras para conservar contraste cuando pasan partículas.

## Movimiento e interacción

El fondo es físico e interactivo, no guarda datos ni emite sonido. Hay hasta 150
partículas con posición, velocidad y radio. Cada una tiene un pequeño núcleo
casi blanco y una caída radial suave de luminosidad, ligeramente teñida.
La composición aditiva hace más luminosos los halos que se superponen; no
pinta discos opacos ni anillos de neón saturado. El perfil de luz se precalcula
al crear el renderer: no hay blur ni exponenciales por fotograma, luces
intermitentes ni estelas. Al entrar a Inicio el
fondo está vacío: nacen una a una desde puntos aleatorios del borde superior,
a unas 11 por segundo, con variación aleatoria entre apariciones. Se utiliza
una semilla nueva en cada entrada al módulo. Caen, rebotan de forma
atenuada, se deslizan y se acumulan contra los bordes según la inclinación.
Un toque libre las remueve; tocar un icono abre el módulo, no la simulación.

Una sacudida superior a 2,5 g inicia un encogimiento y desvanecimiento de
450 ms, limpia el fondo durante 180 ms y reinicia la lluvia gradual con una
entrada suave de 450 ms. Se evita disparar repetidamente el gesto durante dos segundos.
Si el sensor no está disponible, se mantiene la gravedad hacia abajo.

Las cuatro orientaciones requieren una inclinación estable durante 800 ms,
con una zona de histéresis en las diagonales. En Inicio, el reloj, la fecha,
los iconos y la barra de estado giran y se recolocan durante 320 ms con curva
ease-out, por el recorrido angular más corto. Se conserva la pila de partículas.
El teclado, los diálogos críticos y los turnos de voz bloquean temporalmente
el giro; la orientación más reciente sigue disponible al cerrar esos flujos.

El objetivo es **50 fps** (dentro del requisito de 30–60), exclusivamente mientras
Inicio sea visible. Los FPS reales y el comportamiento físico deben verificarse
en la Tab5; el log informa la frecuencia de composición del canvas, no una
medición de la frecuencia física del panel.

## Implementación y ajustes

- M5Unified `0.2.25` inicializa la placa y es el propietario del IMU; el servicio
  de movimiento lee `M5.Imu.getAccel()` en g y publica muestras a la interfaz.
- BMI270 a 100 Hz, acelerómetro ±8 g y giroscopio ±2000 °/s; muestras de servicio
  a 50 Hz en Inicio. El giroscopio se apaga al salir; la orientación automática
  sigue disponible para los módulos.
- Se descartan solo en RAM los offsets cargados por M5 al cambiar el rango,
  evitando mezclar calibraciones de otra escala; no se borran claves de NVS.
- Dos `M5Canvas` RGB565 nativos en PSRAM (aprox. 3,52 MiB entre ambos) componen
  fotogramas completos antes de publicarlos como una imagen LVGL de fondo.
  Se borran regiones ocupadas del canvas trasero, no la pantalla física.
- LVGL dispone de un pool adicional de 2 MiB en PSRAM durante toda la ejecución
  para las capas ARGB del giro de controles. El pool original de 64 KiB no alcanza
  para rotar el reloj; su agotamiento dejaba al dispatcher reintentando y activaba
  el watchdog. Se comprueba el presupuesto libre antes de animar; si la reserva
  falla o queda poca memoria, el giro sigue disponible sin animación. No cambia
  los framebuffers ni la configuración del BSP.
  `CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES=2048` amplía el límite TLSF para
  aceptar ese pool; el pool estático de RAM interna permanece en 64 KiB.
- El halo alcanza hasta 2 px más que el radio físico (máximo 9 px); el borrado y las regiones
  invalidadas incluyen esa extensión para evitar rastros al mover o desvanecer
  partículas. No cambia el radio de las colisiones ni añade buffers de pantalla.
- La imagen decorativa no captura eventos táctiles. Los iconos reciben los
  toques y únicamente los toques libres sobre el fondo remueven las partículas.
- Solo las partículas que cambian de posición, tamaño u opacidad invalidan
  regiones del fondo. Se agrupan en un máximo de 20 rectángulos ajustados,
  uniendo los de menor coste para reservar espacio en la lista de LVGL a reloj
  y estado. Una pila inmóvil no obliga a redibujar toda la pantalla durante la lluvia.
- El BSP conserva pantalla, táctil, PPA, rotación y buffers de LVGL. No se usa
  `pushSprite()` sobre el panel, para evitar un segundo propietario de pantalla.
- El port local mantiene PPA para las tres rotaciones y usa operaciones
  asíncronas con una espera máxima de 100 ms. Se evita la espera indefinida del SRM descrita
  en [IDFGH-18204](https://github.com/espressif/esp-idf/issues/19023) y
  [IDFGH-18292](https://github.com/espressif/esp-idf/issues/19096), sin actualizar
  ESP-IDF ni sustituir el controlador de pantalla. Si falla, se desactiva PPA
  hasta reiniciar y se rota por CPU en un buffer distinto; una terminación DMA
  tardía nunca sobrescribe ese buffer. Los buffers de origen y destino reservados
  al PPA se conservan hasta el reinicio si hubo timeout. Una transferencia rechazada también libera
  la espera de LVGL y registra el error. Este respaldo prioriza interacción
  sobre FPS cuando el acelerador no está disponible.
- Se adapta al port local el ajuste de registro de la
  [corrección oficial de Espressif](https://github.com/espressif/esp-iot-solution/blob/master/components/display/tools/esp_lvgl_adapter/0001-bugfix-ppa-Temporary-fix-for-the-PPA-hang-issue.patch):
  `sr_macro_bk_ro_bypass = 1`. No se modifica la instalación compartida del SDK;
  el ajuste queda dentro del firmware y es específico del ESP32-P4/IDF 5.4.2.
- M5Unified se inicia con una fachada de display externo ya gestionado. Su bus
  interno se reasigna al I²C del BSP antes de iniciar servicios; un mutex recursivo
  protege las transferencias M5 y las llamadas síncronas nativas del IDF.
- Las cuatro orientaciones transforman ambos ejes de gravedad. En posición plana
  se utiliza una gravedad descendente suave en lugar de una escena inmóvil.
  Girar conserva las partículas ya emitidas y su velocidad; no reinicia la lluvia.
- La detección y la orientación aplicada tienen estados separados. Si el BMI270
  deja de entregar muestras se invalidan las lecturas antiguas y se reintenta
  restaurar sus registros de adquisición y mapeo de interrupción, sin resetear
  el sensor ni el bus compartido. Los logs distinguen detección, bloqueo y giro.
- La animación y sus canvas se destruyen al salir de Inicio. No altera el
  cronómetro ni el checkpoint de las misiones.

Constantes ajustables al principio de `firmware/main/home_scene.hpp`:
`MAX_PARTICLES`, `GRAVITY_SCALE`, `BOUNCE_DAMPING`, `SHAKE_THRESHOLD` y
`HOME_FRAME_MS` y `PARTICLE_SPAWN_SECONDS`. La cantidad puede reducirse al construir `HomeScene(N)`.

Pruebas de física sin dispositivo: `sh tools/tests/probar_inicio.sh`. Las mismas
reglas principales también tienen cobertura Unity en el firmware de pruebas.

### Validación de la revisión de física y bloqueo (05/10/2026)

- Pruebas nativas satisfactorias en 1280×720, 720×1280 y dos áreas reducidas:
  nacimiento gradual desde arriba, semillas distintas, giro sin reiniciar,
  gravedad, límites, reposo, toque y sacudida.
- Compilación con ESP-IDF 5.4.2 y carga en la Tab5 sin borrar NVS ni SPIFFS.
- En la prueba por serial: aproximadamente 32–34 fps de composición durante
  la lluvia inicial y 47–48 con la pila en reposo. Se recibió y aplicó un cambio
  de orientación sin detener la animación. No es una garantía de FPS constantes
  en todas las inclinaciones, sacudidas o composiciones.
- Falta la confirmación visual del usuario de los dos iconos y de las cuatro
  orientaciones; el monitor y las pruebas de física no sustituyen esa comprobación.

La paleta negra, las partículas de luz y la transición de orientación se añadieron
después de esa medición. Su aspecto
y frecuencia de composición deben comprobarse de nuevo en la Tab5; no se
atribuyen al nuevo renderizado los FPS de la versión anterior.

### Validación de luz, orientación y energía (06/10/2026)

- Pruebas nativas satisfactorias: física, cuatro orientaciones, espera de 800 ms,
  histéresis, lecturas inválidas, transformación de coordenadas, conversión
  INA226, escala de porcentaje y composición de luz sin desbordes RGB565.
- Compilaciones principal y de pruebas Unity satisfactorias con ESP-IDF 5.4.2.
  Las nuevas pruebas Unity están compiladas, no ejecutadas en hardware.
- En la primera carga, el sensor detectó el giro y la UI lo aplicó, pero faltó
  memoria para las nuevas capas ARGB. Se identificó un bucle de asignación en
  el dispatcher LVGL y se añadió la reserva PSRAM descrita arriba; no se amplió
  el timeout ni se desactivó el watchdog.
- Revisión corregida cargada y verificada por serial: pool PSRAM registrado,
  batería instalada a 7888 mV (estimación 80 %, cargando), y cambios de orientación
  `3 → 2 → 1 → 2 → 3 → 0` aplicados con la transición animada activa, sin repetir
  el watchdog observado en la primera carga. Se aplicaron también giros posteriores
  a `3` y `2` y se registró una sacudida. El canvas informó 45,6 fps inicialmente,
  42–43 fps después de varios giros y una ventana de 19,3 fps tras otro giro.
  Queda pendiente optimizar/verificar las caídas de rendimiento; esta revisión
  no garantiza 30–60 fps constantes durante todas las transiciones.
- Pendiente de confirmar visualmente la suavidad de reloj/iconos y el aspecto
  de las partículas. El registro no sustituye esa evaluación en la pantalla.

## Batería y estados de energía

El INA226 se consulta en una tarea independiente cada cinco segundos. El
registro de voltaje utiliza sus 15 bits positivos completos, con LSB de
1,25 mV; no lleva el desplazamiento de tres bits del INA219.
[Hoja de datos INA226, registro 02h](https://www.ti.com/lit/ds/symlink/ina226.pdf).

- Con batería: `BAT ~87%`; el símbolo `~` indica estimación por voltaje de un
  paquete de dos celdas, siguiendo la escala de M5Unified, no medición de carga.
- Durante carga: `CARGA · BAT ~87%`. CHG_STAT señala carga, no presencia de USB.
- Lectura válida sin batería: `USB · SIN BAT`.
- Fallo de lectura o monitor no disponible: `BAT --%`, sin inventar un 0%.

La precisión varía con carga, consumo y estado de la batería. La conversión
tiene pruebas nativas y Unity; el porcentaje real debe comprobarse con la
NP-F550 instalada en la Tab5.

## Límites

- No copiar gráficos específicos de MotherDuck.
- No añadir misiones, XP, tutorial ni contenido operativo a Inicio.
- No aplicar esta paleta a Productividad o Asistente.
- La hora y la fecha emplean el offset configurado por el usuario.

## Fuente común

Los tokens y reglas compartidas están en
[`docs/ui/lineamientos.md`](../ui/lineamientos.md).
