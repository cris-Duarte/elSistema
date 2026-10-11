# Hábitos — seguimiento diario y continuo

Cuarto módulo de El Sistema, `ModuleId::Habits = 6`. Local, sin micrófono, red ni
XP. No reutiliza los IDs retirados. La primera apertura está vacía y explica
los dos tipos; no se crean datos de demostración.

## Uso

Inicio → **HÁBITOS**. La portada prioriza hoy y la semana actual, con filtros
Todos / Diarios / Continuos. En horizontal muestra lista y detalle; en vertical
tocar un hábito abre su detalle, y Volver recupera la lista.

**Crear / Editar:** nombre obligatorio (80 caracteres UTF-8), descripción
opcional (240), tipo y fecha inicial. El teclado español está anclado abajo y
no incluye MIC. El formulario superior es desplazable y conserva el texto al
cambiar opciones. Guardar confirma solo cuando el worker ha persistido el
documento. Eliminar pide confirmación y elimina también el historial.

### Diarios

- Meta de 1–7 días por semana; valor inicial 5. Semana de lunes a domingo.
- Tocar una casilla marca ese día; tocarla otra vez lo desmarca. Cada fecha
  cuenta una sola vez, desde el comienzo del hábito hasta hoy.
- La portada muestra siete casillas; el detalle añade `n/meta días`, Hecho hoy,
  estado de meta alcanzada y un mosaico de 12 semanas × 7 días.
- El mosaico muestra metas históricas y permite retroceder por bloques de
  doce semanas y corregir días pasados. `X` es cumplido, vacío sin registro,
  borde azul hoy, y `-` fuera del periodo. Una semana inicial parcial lleva
  `*` y explicación; no se reduce su meta.
- Editar permite aplicar la meta esta semana o la próxima. Se conserva la
  historia de metas; las semanas anteriores no se recalculan.
- No hay rachas obligatorias ni etiquetas de fracaso para días sin registro.

### Continuos

- Comienzo local editable, incluso anterior a la creación, convertido a UTC.
- El contador muestra semanas, días y `horas:minutos:segundos`. Se reconstruye
  a partir del RTC: incluye el tiempo fuera del módulo y con la Tab5 apagada.
- Registrar interrupción solicita fecha/hora y confirmación. Cierra el periodo
  y muestra su duración congelada; Retomar abre uno nuevo.
- El detalle conserva periodos y el mejor, sin usar su suma como contador.
- Tocar un periodo permite corregirlo. No se permiten fechas futuras, finales
  anteriores al comienzo ni solapamientos. Un periodo cerrado no se convierte
  silenciosamente en abierto: para abrir uno nuevo se usa Retomar.
- El tipo queda fijo mientras existan registros diarios o periodos.
- Si el RTC no es válido o las fechas quedan en el futuro tras corregir la hora,
  se muestra **Revisar fecha y hora**, sin duraciones ficticias ni avisos.

Las fechas diarias son civiles, no timestamps. Cambiar el offset no mueve los
registros históricos a otro día. Las fechas editables utilizan `AAAA-MM-DD` y
`AAAA-MM-DD HH:MM:SS` (también se acepta HH:MM); se muestran en el offset existente.

## Avisos

Cada diario puede habilitar un aviso, inicialmente desactivado, con hora inicial
18:00 y modo Visual / Tono / Voz. No avisa si ya se marcó hoy, se alcanzó la meta
semanal o se omitió ese día. Los continuos no tienen avisos en esta versión.

El banner no bloqueante dura ocho segundos y ofrece Hecho, +15 min, Omitir hoy
y Ver lista. Los simultáneos se agrupan con una sola señal sonora; las acciones
individuales se refieren al primer pendiente, y la lista permite atender todos.
Después queda un control `AVISO · n`, sin tapar batería o Pausa de Tetris.
Omitir no registra incumplimiento. No hay repetición automática ni sonidos
acumulados; al reiniciar solo se evalúa el día actual y se respeta su recepción
persistida. Posponer habilita una nueva presentación al cumplirse los 15 min.

La voz embebida dice «Tienes un hábito pendiente. Revisa El Sistema». Se genera
localmente con la voz Paulina de macOS mediante `tools/generar_aviso_habitos.sh`;
PCM mono de 16 kHz, sin servicios ni nombres dinámicos. Sonido ofrece volumen
común (30 % inicial), prueba de tono y voz, sin franja de silencio.

El dispositivo encendido despierta la pantalla para presentar un aviso, sin
reanudar juegos. Se aplazan durante formularios, dictado o voz del Asistente.
Productividad tiene prioridad. El banner no pausa Tetris ni una misión; abrir
voluntariamente la lista sale del juego dejándolo pausado. Un fallo de audio
mantiene el aviso visual. AudioSessionManager permite interrumpir **el aviso**
cuando otra actividad de mayor prioridad necesita el codec, nunca a la inversa.

## Arquitectura y archivos

- `habit_model.*`: calendario, registros, metas, periodos, validaciones y
  planificación de pendientes, sin LVGL/FreeRTOS.
- `habit_json.cpp`: codec JSON con validación estricta, versión 1.
- `habit_repository.*`: `/spiffs/habits.json`, `.tmp` y `.bak`, usando el SPIFFS
  ya montado. No vuelve a inicializar NVS, montar ni formatear el almacenamiento.
- `habits.*`: cola de 12 comandos con ownership explícito; worker de dominio y
  persistencia, snapshots y worker de audio independiente. Valida y escribe
  un candidato antes de publicarlo. La recepción del aviso también se persiste
  antes de presentar el banner o pedir audio.
- `ui_habits.cpp`, `habit_calendar.*`: pantallas, contador incremental cada
  segundo, mosaico con 84 casillas de 48×48 px y callbacks de vida acotada.

El documento conserva identificadores, hábitos, registros diarios, metas,
periodos, volumen y estado de avisos. No se escribe cada segundo ni se modifica
el esquema de misiones, las credenciales Wi-Fi o la partida de Tetris.

Las escrituras usan temporal, fflush/fsync, verificación y renombrado. Al cargar
se intenta el principal y luego respaldo. Tras recuperarse, no se sustituye el
respaldo sano por el principal corrupto. Si ambos son inválidos se bloquea la
escritura y se conservan los archivos. Un error de espacio o E/S no confirma el
cambio en RAM; no se borra historial. Se avisa desde el 80 % de ocupación y se
reserva espacio para guardar con seguridad.

## Validación

```sh
sh tools/tests/probar_habitos.sh
sh tools/tests/probar_audio.sh
sh tools/tests/probar_tetris.sh
sh tools/tests/probar_inicio.sh
cmake -S tools/tests/habits_lvgl -B /tmp/elsistema-habits-lvgl
cmake --build /tmp/elsistema-habits-lvgl -j8
/tmp/elsistema-habits-lvgl/habits_lvgl
```

Las pruebas nativas cubren calendario/leap years, metas históricas, UTF-8,
periodos, tiempo apagado, fechas inválidas, avisos omitidos/pospuestos/agrupados,
reloj corregido, serialización, recuperación y fallo de escritura. Las pruebas
LVGL comprueban 84 casillas táctiles, límites, selección y fuentes en ambas
orientaciones. Unity incluye pruebas del dominio para ejecutarlas en la placa.

El repositorio también se prueba con la rama ESP_PLATFORM y un SPIFFS montado
por nombre (`storage`): consultar con etiqueta nula no equivale a consultar
esa partición en ESP-IDF. Audio PCM tiene pruebas de conversión, cancelación,
memoria insuficiente y errores de codec/I2S. El buffer de conversión vive en
PSRAM, no en la pila; el worker de avisos dispone de 12 KiB y registra su mínimo
de pila libre para verificar el margen durante la prueba física.

**Aceptación física pendiente:** compilar no sustituye comprobar en la Tab5 el
teclado, desplazamiento, rotación, legibilidad, RTC tras apagar, voz/tono/volumen,
despertar, avisos simultáneos y convivencia con dictado, Asistente y Tetris.
La primera versión se flasheó y la prueba del usuario detectó Guardar
deshabilitado y salida del módulo al probar audio. Se corrigió la etiqueta de
SPIFFS y se redujo el uso de pila de reproducción, ampliando el worker. Estas
correcciones compilan y pasan las pruebas locales; su carga y repetición de
esas dos pruebas en la Tab5 están pendientes.
