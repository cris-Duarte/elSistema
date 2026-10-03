#!/usr/bin/env bash
#
# Arranca whisper-server y lo anuncia por Bonjour para que la Tab5 lo encuentre.
#
# Uso:  tools/whisper/servidor.sh [--dir ~/whisper.cpp] [--port 8080]
#
# - Usa el endpoint oficial /inference (sin API intermedia).
# - Comprueba que el servidor responde antes de mostrar la IP.
# - No hace falta introducir IP ni puerto en la Tab5.
#
set -euo pipefail

DIR="${WHISPER_DIR:-$HOME/whisper.cpp}"
PORT="8080"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dir) DIR="${2:?}"; shift 2 ;;
        --port) PORT="${2:?}"; shift 2 ;;
        *) echo "Uso: $0 [--dir DIR] [--port PUERTO]" >&2; exit 1 ;;
    esac
done

BIN="$DIR/build/bin/whisper-server"
MODEL="$DIR/models/ggml-small.bin"
[[ -x "$BIN" ]] || { echo "ERROR: no existe $BIN; ejecuta instalar.sh primero." >&2; exit 1; }
[[ -f "$MODEL" ]] || { echo "ERROR: no existe $MODEL; ejecuta instalar.sh primero." >&2; exit 1; }

echo "==> Arrancando whisper-server en 0.0.0.0:$PORT"
"$BIN" -m "$MODEL" --host 0.0.0.0 --port "$PORT" &
SERVER_PID=$!
DNS_SD_PID=""
cleanup() {
    [[ -n "$DNS_SD_PID" ]] && kill "$DNS_SD_PID" 2>/dev/null || true
    kill "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "==> Esperando respuesta del servidor…"
for _ in $(seq 1 30); do
    if curl -fsS "http://127.0.0.1:$PORT/" -o /dev/null 2>/dev/null; then
        break
    fi
    sleep 1
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "ERROR: whisper-server terminó antes de responder." >&2
        exit 1
    fi
done
curl -fsS "http://127.0.0.1:$PORT/" -o /dev/null || {
    echo "ERROR: el servidor no respondió en 30 s." >&2
    exit 1
}

echo "==> Servidor disponible. Preparando anuncio Bonjour…"
LAN_IP=""
if command -v ipconfig >/dev/null 2>&1; then
    DEFAULT_IF="$(route -n get default 2>/dev/null | awk '/interface:/{print $2; exit}' || true)"
    if [[ -n "$DEFAULT_IF" ]]; then
        LAN_IP="$(ipconfig getifaddr "$DEFAULT_IF" 2>/dev/null || true)"
    fi
    if [[ -z "$LAN_IP" ]]; then
        LAN_IP="$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || true)"
    fi
fi
if [[ -z "$LAN_IP" ]]; then
    echo "ERROR: no se encontró una IPv4 LAN activa." >&2
    exit 1
fi

SERVER_ID_FILE="$DIR/.elsistema-server-id"
if [[ ! -s "$SERVER_ID_FILE" ]]; then
    command -v uuidgen >/dev/null 2>&1 || {
        echo "ERROR: macOS no dispone de uuidgen." >&2
        exit 1
    }
    uuidgen | tr '[:upper:]' '[:lower:]' > "$SERVER_ID_FILE"
fi
SERVER_ID="$(tr -d '[:space:]' < "$SERVER_ID_FILE")"
[[ -n "$SERVER_ID" ]] || { echo "ERROR: el identificador Bonjour está vacío." >&2; exit 1; }

COMPUTER_NAME="$(scutil --get ComputerName 2>/dev/null || hostname -s)"
INSTANCE_NAME="El Sistema — $COMPUTER_NAME"
command -v dns-sd >/dev/null 2>&1 || {
    echo "ERROR: no se encontró dns-sd, necesario para Bonjour." >&2
    exit 1
}
dns-sd -R "$INSTANCE_NAME" _elsistema._tcp local "$PORT" \
    "id=$SERVER_ID" "path=/inference" "version=1" >/dev/null 2>&1 &
DNS_SD_PID=$!

echo "    Nombre: $INSTANCE_NAME"
echo "    IP actual: $LAN_IP (la Tab5 la descubrirá automáticamente)"
echo "    endpoint: http://$LAN_IP:$PORT/inference"
echo "Pulsa Ctrl+C para detener el servidor."
wait $SERVER_PID
