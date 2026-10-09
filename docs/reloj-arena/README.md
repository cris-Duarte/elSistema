# Módulo Reloj de arena

El tercer módulo de El Sistema reproduce un reloj de arena con luz dorada.
Funciona localmente y mide tiempo transcurrido. La duración elegida calibra el
vaciado en vertical: manipular la Tab5 cambia el flujo y puede prolongar la sesión.

## Uso

Desde Inicio, tocar **RELOJ DE ARENA**. En el primer uso se elige un intervalo
de 1 a 120 minutos mediante presets o una rueda; el valor inicial es 25 minutos.
**Iniciar** exige una posición vertical y coloca la arena en la cámara
físicamente superior, también si el dispositivo está invertido.

La vista inmersiva muestra solo el cristal y el cronómetro. Un toque revela
Pausar/Reanudar, Reiniciar, Duración e Inicio. Los controles se ocultan a los
cuatro segundos mientras corre la sesión y no existe un diálogo o error abierto.
Pausar congela tanto el tiempo como la arena; mostrar controles permite seguir
midiendo. Reiniciar pide confirmación y deja el reloj listo para un inicio
explícito. Cambiar duración pausa; cancelar conserva la sesión pausada.

La orientación del panel queda bloqueada en 720×1280, incluso al girar a 90°,
180° o 270°. El IMU cambia la gravedad de la arena, no la orientación del cristal
ni del texto. De lado o plano, el flujo puede detenerse mientras el cronómetro
continúa. Las sacudidas no eliminan arena.

Invertir durante una sesión conserva el tiempo y cambia el sentido de caída.
Después del vaciado y asentamiento, el cronómetro se detiene. Una inversión
estable de 800 ms inicia otra medición desde cero usando la arena existente.
Salir o apagar la pantalla pausa y desarma esa inversión automática hasta
continuar explícitamente. Volver a Inicio recupera la orientación física actual.

Los recordatorios de Productividad permanecen pendientes en la vista inmersiva
y se muestran al abrir controles o salir. Retomar una misión conduce a
Productividad. Su cronómetro y checkpoints siguen funcionando durante el reloj.

## Implementación

- `hourglass_model.*`: motor determinista sin LVGL, tiempo monotónico, perfiles
  de acumulación, cantidad conservada y registro binario versionado con checksum.
- `hourglass.*`: tarea FreeRTOS propietaria del motor, cola de acciones,
  confirmación numerada de comandos y snapshots; ningún worker llama a LVGL.
- `hourglass_repository.*`: namespace NVS `hourglass`, clave `session`.
  Guarda cada transición y cada 30 segundos durante una sesión; reintenta si
  falla NVS. El JSON de misiones no incorpora estos datos.
- `hourglass_raster.*`: compositor RGB565 portable, compartido por firmware y
  pruebas visuales nativas. Geometría y luz precalculadas, textura granulada y
  chorro confinado al cristal.
- `hourglass_renderer.*`: dos canvas M5Canvas de 480×1040 en PSRAM
  (aproximadamente 1,90 MiB juntos), publicados como imagen LVGL. LVGL es el
  único propietario del flush físico.

La simulación es una aproximación bidimensional híbrida. Mantiene volumen
continuo por cámara; la gravedad define perfiles de superficie y condiciona
la alimentación del cuello. El caudal nominal es `1 / duración`; se reduce
según la componente longitudinal y se bloquea cuando la arena no alimenta el
cuello. Los puntos visibles representan material y movimiento, sin cuantizar
una sesión de dos horas en un número pequeño de partículas.

El servicio del BMI270 publica muestras a 50 Hz en Inicio y en este módulo.
El reloj utiliza los ejes del panel vertical fijo; no transforma las muestras
según la autorrotación detectada. Si desaparecen, pausa y exige reanudación
explícita. No inventa gravedad cuando la Tab5 está plana.

NVS conserva duración, estado anterior a pausa, tiempo, proporción de arena,
gravedad de referencia y sentido de finalización. Al reiniciar, una sesión
activa o finalizada se recupera pausada. Se reconstruye la superficie sin
guardar cada grano; el tiempo apagado no se suma. Un registro inválido se
rechaza y se informa en el log, sin borrar datos de otros módulos.

## Validación

Ejecutar `sh tools/tests/probar_reloj_arena.sh` para probar duraciones de
1/25/120 minutos, inversión, conservación, posición plana, pausa, pérdida del
sensor, checksum y recuperación. Acepta una ruta `.ppm` opcional para renderizar
una muestra del mismo compositor usado en la Tab5.

Las pruebas Unity del firmware añaden recuperación y un benchmark de canvas
(presupuesto menor de 25 ms/fotograma, sin incluir flush LVGL). El log de la vista
muestra FPS de composición y cantidades cada diez segundos. El criterio de
aceptación del módulo es al menos 30 FPS durante su uso en la Tab5.

La aceptación manual requiere comprobar presets y rueda, tacto y ocultación,
inversiones físicas, pantalla apagada, navegación, reinicio, recordatorios y
fluidez. Un benchmark de raster no sustituye medir la vista completa en el equipo.

## Diseño

Consultar [design.md](design.md). No tiene sonido, red, XP ni vinculación con
misiones; su única dependencia física es el IMU compartido.
