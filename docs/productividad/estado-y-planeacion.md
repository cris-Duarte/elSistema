# Módulo de productividad: estado y planeación

**Estado:** MVP local implementado; dictado por voz integrado.  
**Firmware de referencia:** v0.4.2 · ESP-IDF 5.4.2 · M5Stack Tab5.

Este documento describe el módulo de productividad que existe en el firmware y
propone cómo cerrar su validación y evolucionarlo. Las misiones y su historial
son locales; el dictado necesita además Wi-Fi y el servidor Whisper del
MacBook. El módulo sigue siendo utilizable sin red mediante el teclado táctil.

## Qué tiene actualmente

### Misiones

- Crear y editar misiones con título, descripción, rango `E–S` y recordatorio
  configurable. El título admite hasta 80 caracteres y la descripción hasta
  240; el recordatorio se puede apagar o configurar entre 1 y 120 minutos.
- El formulario usa un teclado táctil en español y conserva los caracteres
  acentuados y la `ñ`.
- El dictado puede proponer el título de una misión. El texto reconocido se
  abre en el formulario para revisión y edición; la misión no se guarda hasta
  confirmar **Guardar**. Si no hay conectividad, el registro escrito continúa
  disponible.
- Las misiones completadas son elementos del historial, no se reabren como
  tareas activas. Se pueden consultar y eliminar con confirmación.

### Zona de Enfoque, cronómetro y recordatorios

- La Zona de Enfoque permite agregar, retirar y ordenar misiones pendientes.
- Solo una misión puede estar activa. Al iniciar otra, la anterior se pausa y
  su intervalo queda registrado. Al pausar manualmente también se conserva el
  tiempo acumulado.
- El cronómetro de la misión activa es el elemento principal de la pantalla;
  utiliza una fuente de 128 px. Se presentan primero los metadatos, después el
  cronómetro, el título y la descripción.
- Si no hay misión activa, la pantalla principal destaca la lista de la Zona
  de Enfoque para facilitar la elección de la siguiente.
- Los paneles de Zona de Enfoque y Backlog se pueden abrir y cerrar desde tabs
  laterales. En horizontal se puede consultar la misión junto a esos paneles;
  en vertical se conserva una composición compacta.
- Los recordatorios al pausar una misión se muestran visualmente y se ponen
  en cola. El usuario puede retomarla, posponer el aviso cinco minutos o
  cancelar ese recordatorio. No hay todavía anuncios hablados para estos
  recordatorios.

### Recompensas e historial

- Completar una misión cierra su intervalo y concede XP según el rango y el
  tiempo activo: `XP base del rango + 2 × minutos activos`, con el componente
  temporal limitado a 120 minutos. Bases: `E=25`, `D=50`, `C=100`, `B=200`,
  `A=350`, `S=600`.
- La progresión admite varios ascensos de nivel por una sola recompensa. La
  XP histórica concedida no se retira al eliminar una misión completada.
- Al completar no se inicia otra misión automáticamente; la interfaz ofrece
  iniciar la siguiente o volver.
- El historial permite buscar por título o descripción sin distinguir
  mayúsculas ni acentos, filtrar por rango y fecha, ordenar por fecha de
  finalización y consultar duración, intervalos y XP.

### Persistencia, reloj y orientación

- El estado de misiones se guarda como JSON con versión de esquema en SPIFFS,
  usando archivo principal, temporal y respaldo. NVS guarda preferencias y el
  checkpoint del cronómetro.
- La aplicación guarda checkpoints aproximadamente cada 30 segundos. Tras
  reiniciar recupera la misión activa como pausada y conserva el último
  checkpoint, sin contar el tiempo que la placa estuvo apagada.
- El primer inicio solicita configurar fecha y hora en el RTC RX8130CE; las
  duraciones activas se miden con reloj monotónico. La rotación automática usa
  BMI270 y espera una orientación estable antes de cambiar el diseño. Se
  bloquea durante el teclado y diálogos.
- Inicio es ahora el lanzador de módulos. El tutorial de Productividad aparece
  al entrar por primera vez a ese módulo, no cubre el lanzador. Las misiones de
  ejemplo siguen apareciendo cuando todavía no existe un estado guardado y se
  pueden eliminar.

### Componentes principales

| Componente | Responsabilidad |
| --- | --- |
| `model.*` | Misiones, intervalos, Zona de Enfoque, perfil y preferencias. |
| `services.*` | Reglas de misiones, exclusión del cronómetro, recordatorios y XP. |
| `repository.*` | Serialización JSON, SPIFFS, preferencias NVS y checkpoint. |
| `hardware.*` | RTC, presentación de fecha/hora y orientación BMI270. |
| `audio.*`, `audio_session.*` | Captura/reproducción y acceso exclusivo al codec. |
| `dictation.*`, `transcription.*` | Captura, estados de dictado y envío a Whisper. |
| `connectivity.*` | Redes conocidas y descubrimiento Bonjour del servidor Mac. |
| `ui.*` | Pantallas LVGL, formularios y traducción de toques a acciones de servicio. |

La interfaz consulta el estado central y delega las mutaciones en los
servicios; no es la propietaria de las reglas del dominio. El dictado comparte
audio y conectividad con el módulo de voz, pero su integración con
productividad termina en entregar texto al editor de misión.

La especificación visual del módulo está en
[`design.md`](design.md); el contexto compartido de navegación y estilo está en
[`docs/inicio/design.md`](../inicio/design.md) y
[`docs/ui/lineamientos.md`](../ui/lineamientos.md).

## Límites y riesgos conocidos

- El recordatorio de pausa es visual: aún no anuncia por voz ni tiene ajustes
  de volumen o silencio.
- La fecha se guarda como UTC y se presenta usando `utc_offset_minutes`. El
  nombre de zona, por ejemplo `America/Asuncion`, se conserva como preferencia,
  pero el firmware no calcula automáticamente cambios de horario de verano.
- La persistencia tiene un formato versionado y recuperación desde respaldo,
  pero el esquema actual es la versión 1; todavía no existe una cadena de
  migraciones para futuras versiones de datos.
- La inicialización de SPIFFS está configurada con
  `format_if_mount_failed=true`. Antes de considerar el almacenamiento
  endurecido, debe revisarse este comportamiento para evitar que una falla de
  montaje pueda terminar en formateo automático. El README advierte que no se
  debe reformatear SPIFFS durante las pruebas porque allí están los datos del
  usuario.
- La aplicación bloquea nuevas creaciones cuando el espacio usado alcanza el
  umbral configurado y las escrituras verifican errores, pero hace falta
  probar explícitamente el comportamiento con el almacenamiento casi lleno y
  con fallas de escritura en cada transición.
- El dictado depende de que la red local y `tools/whisper/servidor.sh` estén
  disponibles. El teclado siempre es la alternativa y la captura puede
  reproducirse localmente para diagnóstico.
- No hay todavía exportación/restauración de datos, sincronización entre
  dispositivos ni actualización OTA.

## Planeación para lo pendiente

Las siguientes etapas son una propuesta de continuación del módulo; no
significan que haya cambios ya implementados.

### P1 · Cerrar la validación del MVP

1. En la Tab5, probar crear, editar, pausar, alternar, completar y eliminar
   misiones con títulos y descripciones que incluyan tildes, `ñ` y límites de
   longitud.
2. Confirmar que al alternar repetidamente nunca hay dos cronómetros activos
   y que cada intervalo aparece correctamente en el detalle del historial.
3. Reiniciar durante una misión en marcha y comprobar el checkpoint, la
   recuperación pausada y que no se suma tiempo durante el apagado.
4. Probar recompensas de todos los rangos, límites temporales y varios
   ascensos de nivel.
5. Probar recordatorios vencidos simultáneamente, reanudación, posposición,
   cancelación y persistencia después de reiniciar.
6. Recorrer búsqueda, filtros, orden y detalle del historial con datos reales.
7. Probar el diseño vertical y horizontal en la placa, incluida la apertura de
   teclado y diálogos mientras se solicita rotación.
8. Probar JSON principal corrupto con respaldo válido, ambos archivos
   inválidos, volumen casi lleno y fallo de escritura. Antes de estas pruebas,
   conservar una copia de los datos y revisar la opción de autoformateo de
   SPIFFS.

**Salida:** una lista reproducible de pruebas en hardware con resultados y
problemas pendientes identificados.

### P2 · Endurecer persistencia y fechas

- Desactivar el formateo automático ante fallas de montaje y presentar un
  estado de recuperación que preserve los archivos existentes.
- Definir migraciones de esquema explícitas antes de añadir campos al modelo.
- Verificar que todas las mutaciones revierten correctamente el estado en RAM
  si no se logra guardar en SPIFFS.
- Decidir si basta un offset UTC fijo o si se necesita una base de zonas
  horarias con reglas de calendario. Si se requiere ajuste automático,
  implementarlo y probar cambios de fecha y zona.
- Definir una política observable para poco espacio: aviso, bloqueo seguro y
  recuperación manual sin borrar misiones ni XP automáticamente.

**Salida:** reinicios, fallas de almacenamiento y cambios de versión no
comprometen silenciosamente el historial.

### P3 · Avisos hablados locales

1. Reutilizar el arbitraje de `AudioSessionManager` para que un recordatorio no
   interrumpa dictado, reproducción o el asistente.
2. Añadir una frase breve en español al vencer un recordatorio, conservando el
   aviso visual y las acciones táctiles.
3. Añadir preferencias de voz, volumen y silencio, con voz desactivada como
   alternativa clara.
4. Si el audio está ocupado, mantener el aviso pendiente sin bloquear la
   interfaz ni perderlo.

**Salida:** los avisos se oyen cuando es seguro hacerlo y nunca detienen el
cronómetro ni descartan una grabación.

### P4 · Copias de seguridad, OTA y endurecimiento

- Añadir una copia exportable del estado de productividad y un flujo de
  restauración con validación de esquema y confirmación.
- Planear OTA con verificación de imagen y recuperación ante actualización
  interrumpida.
- Revisar manejo de errores, límites de memoria, integridad de datos y
  comportamiento tras pérdida de energía.
- Documentar qué información sale de la placa durante el dictado y qué
  permanece local; conservar el modo teclado sin red.

**Salida:** actualización y recuperación controladas sin perder el estado
personal.

## Criterios para considerar cerrado el MVP

- Todas las transiciones de misión y cronómetro sobreviven reinicios sin
  duplicar tiempo ni activar dos misiones.
- XP, intervalos y nivel coinciden con las reglas documentadas.
- Las misiones completadas se pueden encontrar y consultar después de varios
  reinicios.
- Los fallos de RTC o almacenamiento no provocan pérdida silenciosa del
  historial y son comunicados claramente.
- Los recordatorios visuales y el teclado siguen disponibles aunque falle el
  audio, Wi-Fi o Whisper.
- Ambas orientaciones son legibles y táctiles en el Tab5 real.

## Archivos relacionados

- Estado general y pasos de compilación: `firmware/README.md`.
- Modelo de datos: `firmware/main/model.hpp` y `firmware/main/model.cpp`.
- Reglas de productividad: `firmware/main/services.hpp` y
  `firmware/main/services.cpp`.
- Persistencia y checkpoint: `firmware/main/repository.hpp` y
  `firmware/main/repository.cpp`.
- RTC y orientación: `firmware/main/hardware.hpp` y
  `firmware/main/hardware.cpp`.
- Interfaz, historial, formularios y recordatorios: `firmware/main/ui.hpp` y
  `firmware/main/ui.cpp`.
- Dictado: `firmware/main/dictation.*`, `firmware/main/audio.*`,
  `firmware/main/transcription.*` y `tools/whisper/`.
- Estado y planeación del asistente separado: `docs/asistente/estado-y-planeacion.md`.
