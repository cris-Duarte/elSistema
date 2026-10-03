# Servidor local de dictado (macOS + Tab5)

El dictado es privado: la Tab5 graba y envía el WAV a este MacBook en la
misma red local. No hay nube ni cuentas.

## Requisitos

- macOS con Metal, `git`, `cmake`, `curl`, `shasum`.
- La Tab5 y el Mac en la misma red privada.

## Instalación (una vez)

```bash
tools/whisper/instalar.sh
```

Fija `whisper.cpp v1.9.4`, compila con Metal y descarga `ggml-small.bin`
(multilingüe, ~466 MB, SHA `55356645…`). No usa `small.en`, que es solo
inglés y degradaría el español.

## Arranque (cada sesión)

```bash
tools/whisper/servidor.sh
```

Escucha en `0.0.0.0:8080`, espera a responder `GET /` y entonces muestra la
IP LAN actual. Después publica por Bonjour el servicio `_elsistema._tcp` con
un identificador estable, la ruta y la versión del protocolo. La Tab5 lo
descubre automáticamente, incluso si la dirección IP del Mac cambia, y usa
directamente `POST http://<ip-resuelta>:8080/inference`.

El identificador se genera una sola vez en
`$WHISPER_DIR/.elsistema-server-id`. Si hay varios Macs anunciando el servicio,
la pantalla `CONECTIVIDAD → MACBOOK` permite elegir cuál recordar.

## Notas

- HTTP local sin autenticación ni exposición a Internet (v0.5.0 endurece).
- Bonjour necesita que la red permita tráfico multicast entre la Tab5 y el Mac.
- El arranque automático con macOS queda fuera de este hito.
- Los binarios y el modelo viven en `~/whisper.cpp/` y no se versionan.
