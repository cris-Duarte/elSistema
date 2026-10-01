# El Sistema

Firmware para el kit de desarrollo **M5Stack Tab5 (ESP32-P4)**. El primer
módulo implementa una interfaz de productividad basada en misiones, Zona de
Enfoque, medición de tiempo y recompensas de experiencia.

El proyecto utiliza:

- ESP-IDF 5.4.2
- LVGL 9.2.2
- BSP de M5Stack Tab5
- Pantalla táctil MIPI-DSI en orientación horizontal

El código del dispositivo y las instrucciones de compilación están en
[`firmware/`](firmware/).

## Estado actual

- Inicialización del display y panel táctil.
- Dashboard nativo de El Sistema.
- Cronómetro monotónico por misión.
- Pausar, reanudar, alternar y completar misiones.
- Cálculo de XP.
- Fuentes con soporte Latin-1 para textos en español.

La persistencia, creación de misiones, historial completo y servicios de audio
se incorporarán en las siguientes iteraciones.
