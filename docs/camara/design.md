# Diseño — Cámara

## Identidad

Monitor sobrio, negro `#000000`, texto blanco suave `#E8EDF0`, información
secundaria azul grisácea `#A5C2D1` y controles sobre `#10151C` al 90 %.
Rajdhani para títulos/métricas y fuentes españolas existentes. Sin partículas,
neón, ornamentos HUD ni sonido. La imagen tiene prioridad absoluta.

## Composición de la primera entrega

Horizontal fija 1280×720; imagen 16:9 a tamaño nativo y sin distorsión/recorte.
La barra global no se dibuja dentro del monitor. Barra de controles de 122 px
superpuesta arriba, sin cambiar el tamaño del video: Inicio, Conexión,
Reintentar y Ocultar. Fila inferior de métricas en la misma barra.

Con señal, un toque revela controles durante 5 s. El panel de conexión puede
permanecer abierto para la prueba y tiene desplazamiento para los formatos.
Sin señal, una superficie negra cubre la última imagen: título de estado e
instrucción legible. No se deja una captura detenida aparentando estar en vivo.
Las alertas de otros módulos conservan prioridad. Los botones nunca son menores
de 48 px; no hay gestos ocultos para volver a Inicio.

Inicio conserva su aspecto actual. El icono de Cámara utiliza un cuerpo con
objetivo dibujado por código, en un cuadrado azul grisáceo con contorno oscuro.
ID 10; cinco iconos en horizontal y filas centradas de tres/dos en vertical.

## Ayudas pendientes tras validar hardware

Guías finas y de contraste dual, centro y tercios seleccionables. Formatos
9:16 y 1:1 solo delimitan encuadre; no ocultan o estiran la imagen exterior.
Espejo inicialmente apagado. Ampliación etiquetada, vuelta explícita a 1× y
centrado accesible. No confundir esta ampliación digital de una señal 720p con
detalle 4K o con zoom/enfoque controlado desde Sony.

No usar punto rojo, temporizador REC ni batería de cámara sin datos reales.
El estado «En vivo» describe exclusivamente la recepción USB.
