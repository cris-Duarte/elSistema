# El Sistema

Firmware local para el **M5Stack Tab5 (ESP32‑P4)**. El módulo de productividad
convierte tareas en misiones, conserva el tiempo real de trabajo, administra
una Zona de Enfoque y concede experiencia al completar objetivos.

## Estado: v0.4.2

El MVP ya incluye creación y edición de misiones, teclado español, cronómetro
por intervalos, recordatorios visuales, XP y niveles, historial consultable,
persistencia SPIFFS/NVS, RTC y rotación automática. Funciona completamente sin
red.

v0.3.0 añade captura local (ES7210 TDM → PCM mono 16 kHz en PSRAM, hasta 30 s,
con parada por silencio, detección dinámica de slots y reproducción por ES8388,
incluso antes de configurar la red). v0.4.0 añade dictado
privado: la Tab5 se conecta por Wi-Fi (ESP-Hosted/SDIO) al MacBook en la red
local y transcribe con `whisper.cpp` (`tools/whisper/`); el resultado abre el
editor con el título reconocido y editable.

v0.4.1 incorpora un administrador de conectividad: recuerda hasta ocho redes,
elige por prioridad y señal, se reconecta en segundo plano y descubre el Mac
mediante Bonjour/mDNS. Ya no se configura una IP fija; si el Mac cambia de
dirección, la siguiente transcripción vuelve a resolverla automáticamente.

v0.4.2 incorpora el primer turno de voz del asistente: escucha y transcripción
reales con una respuesta hablada local, todavía sin IA conversacional ni
funciones sociales.

La configuración estable del display MIPI‑DSI, táctil, PSRAM, rotación PPA y
fuentes españolas se mantiene dentro del proyecto ESP-IDF 5.4.2.

Consulta [firmware/README.md](firmware/README.md) para arquitectura,
compilación, flasheo y validación en el dispositivo.

## Próximas versiones

- `v0.5.0`: copias de seguridad, OTA y endurecimiento de seguridad.
