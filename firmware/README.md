# El Sistema — firmware para M5Stack Tab5

Primera base nativa del módulo de productividad. Usa ESP-IDF 5.4.2, el BSP de
M5Stack Tab5 y LVGL 9.2.2.

## Alcance de esta iteración

- Inicialización del display MIPI-DSI y del panel táctil.
- Dashboard horizontal inspirado en el mockup web.
- Misión activa con cronómetro real.
- Pausar, reanudar, completar y cambiar de misión.
- Cálculo inicial de XP por rango y tiempo activo.
- Diálogos táctiles para recompensa y funciones aún no implementadas.

La persistencia en NVS, el teclado, el audio y el dictado se añadirán como
servicios separados en las próximas iteraciones.

## Compilar

Activa primero el entorno de ESP-IDF 5.4.2 mediante EIM, la extensión de
VS Code o el script de activación correspondiente a tu instalación.

```bash
cd firmware
idf.py build
```

## Flashear

```bash
idf.py -p PORT flash monitor
```

Para salir del monitor: `Ctrl + ]`.
