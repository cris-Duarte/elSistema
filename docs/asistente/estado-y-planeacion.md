# Asistente de El Sistema: estado y planeación

**Estado del trabajo:** en pausa después del primer bucle de voz A2.  
**Firmware de referencia:** v0.4.2 · ESP-IDF 5.4.2 · M5Stack Tab5.

Este documento resume lo que existe en el repositorio y propone el trabajo pendiente para convertir el módulo visual en un asistente conversacional. La conversación con IA y las funciones sociales no están implementadas.

## Qué tiene actualmente

### A0 · Conectividad y arbitraje de audio

- El Tab5 inicia BLE sobre el ESP32-C6/ESP-Hosted y anuncia `El Sistema`.
- Tiene un servicio de diagnóstico BLE para comprobar conexión y mensajes ping. No implementa funciones sociales ni una conversación por BLE.
- `AudioSessionManager` concede el codec a un solo consumidor a la vez. Reconoce captura y reproducción del dictado, captura y reproducción del asistente, recordatorios y diagnóstico.

### A1 · Personaje y estados visuales

- Personaje gráfico original integrado como recurso LVGL; ojos, boca, núcleo, aura y movimiento se dibujan o animan en la interfaz.
- Estados disponibles: reposo, saludo, atención, escucha, procesamiento, respuesta y celebración.
- Se conservan acciones de demostración visual para probar el personaje sin iniciar un turno de voz.
- La máquina de estados vive en `assistant.*`; el código no depende de LVGL.

### A2 · Primer turno de voz

Desde `ASISTENTE`, el botón **HABLAR** inicia una captura con los micrófonos de la Tab5. El nivel de entrada se muestra en la pantalla. La captura se puede detener con **DETENER** o cancelar; cuando falla la conectividad, el audio se conserva en PSRAM para reintentar.

La transcripción reutiliza la ruta de dictado ya existente: Wi-Fi del ESP32-C6, descubrimiento del MacBook por Bonjour y `POST /inference` al servidor local `whisper.cpp`. Al recibir el texto, el personaje pasa al estado de respuesta y reproduce por el ES8388 la frase fija «Te escucho. El canal de voz está listo.».

La frase de audio está incluida en el firmware como PCM mono de 16 kHz. No se genera a partir de la transcripción ni de un modelo de lenguaje. La respuesta se muestra junto con el texto reconocido durante la sesión de la interfaz; no se guarda como historial conversacional.

### Componentes principales

| Componente | Responsabilidad actual |
| --- | --- |
| `assistant.*` | Estados visuales y expresiones del personaje. |
| `assistant_voice.*` | Coordina el turno, observa el estado del dictado y solicita la respuesta fija al worker de audio. |
| `dictation.*` | Captura audio, controla niveles, gestiona errores y coordina la transcripción existente. |
| `audio.*` y `audio_session.*` | Captura/reproducción del codec y exclusión mutua de sus consumidores. |
| `transcription.*` | Envío HTTP a Whisper y análisis de la respuesta JSON. |
| `connectivity.*` | Perfiles Wi-Fi y descubrimiento del servidor en la red local. |
| `ui.*` | Controles táctiles, personaje, indicador de nivel y estados del turno. |

La especificación visual del módulo está en [`design.md`](design.md). La
cabecera de Asistente permite volver a Inicio; al salir se cancela el turno de
voz. La guía común explica cómo convive este HUD oscuro con el lanzador claro:
[`docs/ui/lineamientos.md`](../ui/lineamientos.md).

## Límites conocidos

- El asistente no interpreta la frase: Whisper solo la convierte en texto.
- La respuesta es siempre la misma frase pregrabada; no es voz sintetizada en tiempo real.
- No hay diálogo de varios turnos, contexto, memoria, personalidad configurable ni herramientas para operar misiones.
- La transcripción requiere Wi-Fi, el MacBook accesible y `tools/whisper/servidor.sh` activo. Si falla el servidor, se puede reintentar el envío con el audio retenido mientras la muestra siga disponible.
- El módulo no escucha continuamente ni tiene palabra de activación. Cada turno comienza mediante el control táctil.
- No hay acciones sociales, contactos, mensajería, publicación de datos ni actuadores/servos.
- El build normal y el build de pruebas compilaron; las pruebas de dominio informaron 21 casos y 0 fallidos. El firmware normal v0.4.2 fue flasheado y su inicio, Wi-Fi y descubrimiento Bonjour se observaron por serie. Falta registrar la validación física completa de captura → transcripción → respuesta por altavoz con la versión normal.

## Planeación para lo pendiente

Las etapas se pueden retomar una por una. La implementación de IA queda pausada hasta decidir dónde se ejecutará y qué datos se enviarán.

### A2.1 · Cerrar la validación física

1. Iniciar el servidor Whisper en el MacBook y comprobar un turno completo desde **HABLAR** hasta la frase por el altavoz.
2. Confirmar que la transcripción aparece correctamente con tildes, `ñ` y términos técnicos.
3. Probar servidor apagado, pérdida de Wi-Fi, reintento y cancelación.
4. Repetir con una misión activa y verificar que el cronómetro continúa.
5. Probar orientación horizontal y vertical, interacción táctil y diez turnos seguidos sin congelamiento ni pérdida progresiva de memoria.
6. Anotar cualquier error de audio o transcripción antes de añadir un modelo conversacional.

**Salida:** una prueba reproducible del recorrido de voz y un comportamiento de error aceptable.

### A3 · Respuesta conversacional

1. Elegir ejecución local en el MacBook o un proveedor alojado. La opción local mantiene el texto en la red privada, pero exige un servicio y un modelo disponibles en el Mac; un proveedor alojado puede ofrecer mejor disponibilidad/calidad, pero envía el texto transcrito fuera de la red y necesita gestión de credenciales.
2. Mantener Whisper como servicio de reconocimiento independiente. Añadir una interfaz de conversación separada que reciba texto y devuelva texto, estado y error.
3. Definir en español el alcance, la personalidad, los límites y el formato de respuesta del asistente.
4. Implementar timeout, cancelación, reintento sin volver a grabar, respuesta vacía o inválida y fallback visible cuando el servicio no esté disponible.
5. Reflejar con precisión los estados en el personaje: escucha, transcripción, generación, respuesta y error.
6. No ejecutar acciones sobre misiones durante esta etapa: primero validar una conversación de texto completa.

**Salida:** el usuario habla, revisa su transcripción y recibe una respuesta contextual visible en pantalla.

### A4 · Voz de salida dinámica

1. Comparar síntesis de voz local en el MacBook con síntesis alojada, usando privacidad, naturalidad, latencia y complejidad como criterios.
2. Definir un formato de audio acotado que la Tab5 pueda reproducir por ES8388, preferentemente PCM mono con frecuencia conocida.
3. Enviar la respuesta de texto a síntesis y reproducir el resultado sin bloquear LVGL.
4. Actualizar la animación de boca y estado de voz mientras se reproduce; permitir interrumpir la respuesta.
5. Mantener la frase PCM integrada como aviso de fallback, no como respuesta normal.

**Salida:** cada respuesta conversacional también se escucha por el altavoz.

### A5 · Conversación y uso del contexto

1. Diseñar cómo se ve el intercambio de turnos y cómo se inicia el siguiente turno.
2. Empezar con contexto volátil en RAM y borrar la conversación al cerrar el módulo o reiniciar.
3. Definir explícitamente qué información de productividad se puede compartir con el asistente y pedir consentimiento antes de incluir misiones, títulos o historial.
4. Diseñar comandos de productividad como una etapa separada: interpretar intención, mostrar una acción propuesta y solicitar confirmación antes de crear, editar, iniciar o completar una misión.
5. Añadir persistencia de conversación únicamente si se decide su valor y su política de retención.

**Salida:** diálogo útil con acceso gradual y explícito al contexto personal.

### A6 · Endurecimiento y evaluación

- Evitar que una respuesta generada ejecute directamente una acción sin confirmación de la persona.
- Limitar tamaño de texto/audio, tiempos de espera, reintentos y memoria.
- No incluir claves ni contraseñas en firmware, logs o snapshots visibles.
- Antes de acceso fuera de la LAN, diseñar autenticación, cifrado de transporte y protección de credenciales.
- Probar acentos, ruido, voz baja, desconexiones, respuestas malformadas, cancelación durante cada etapa y uso sostenido.
- Mantener las funciones sociales fuera del alcance hasta que se definan por separado.

## Criterios para considerar listo el asistente conversacional

- La Tab5 completa un ciclo de voz sin bloquear pantalla, controles ni cronómetro de misiones.
- El texto reconocido puede revisarse y corregirse antes de enviarse al generador de respuesta.
- La interfaz distingue claramente transcripción, generación y reproducción.
- Los fallos preservan la posibilidad de reintentar o cancelar sin perder datos de productividad.
- La política sobre proveedor, privacidad y retención está visible y corresponde al comportamiento real.
- Ninguna acción sobre misiones se realiza sin confirmación explícita.

## Archivos relacionados

- Implementación del personaje: `firmware/main/assistant.hpp` y `firmware/main/assistant.cpp`.
- Turno de voz: `firmware/main/assistant_voice.hpp` y `firmware/main/assistant_voice.cpp`.
- Captura y reproducción: `firmware/main/audio.hpp` y `firmware/main/audio.cpp`.
- Whisper: `firmware/main/transcription.*`, `firmware/main/dictation.*` y `tools/whisper/`.
- Estado general de firmware: `firmware/README.md`.
