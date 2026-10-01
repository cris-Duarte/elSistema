# El Sistema

Firmware local para el **M5Stack Tab5 (ESP32‑P4)**. El módulo de productividad
convierte tareas en misiones, conserva el tiempo real de trabajo, administra
una Zona de Enfoque y concede experiencia al completar objetivos.

## Estado: v0.2.0

El MVP ya incluye creación y edición de misiones, teclado español, cronómetro
por intervalos, recordatorios visuales, XP y niveles, historial consultable,
persistencia SPIFFS/NVS, RTC y rotación automática. Funciona completamente sin
red.

La configuración estable del display MIPI‑DSI, táctil, PSRAM, rotación PPA y
fuentes españolas se mantiene dentro del proyecto ESP-IDF 5.4.2.

Consulta [firmware/README.md](firmware/README.md) para arquitectura,
compilación, flasheo y validación en el dispositivo.

## Próximas versiones

- `v0.3.0`: captura/reproducción y avisos hablados locales.
- `v0.4.0`: dictado español privado con Whisper en la red local.
- `v0.5.0`: copias de seguridad, OTA y endurecimiento de seguridad.
