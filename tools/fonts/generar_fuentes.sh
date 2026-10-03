#!/usr/bin/env bash
#
# Genera las fuentes embebidas de El Sistema.
#
#   Rajdhani SemiBold  -> voz del Sistema (titulos, rotulos, cifras, teclas)
#   Poppins Regular    -> contenido del usuario y texto corrido
#
# Los TTF no se versionan (son ~1 MB de binarios): se descargan aqui y se
# verifican por SHA256, de modo que la procedencia queda fijada y el resultado
# es reproducible. Las fuentes generadas (.c) si se versionan.
#
# Uso:  tools/fonts/generar_fuentes.sh
#
set -euo pipefail

RAJDHANI_URL="https://raw.githubusercontent.com/google/fonts/main/ofl/rajdhani/Rajdhani-SemiBold.ttf"
RAJDHANI_SHA="94bbd25a18ca665999feb05a537de9fd2b860dcfb78bbe9ca00270825bf235da"
POPPINS_URL="https://raw.githubusercontent.com/google/fonts/main/ofl/poppins/Poppins-Regular.ttf"
POPPINS_SHA="7e65201e9b79159e2300267cc885e16c8dcef2424cdfa09a29bfb0980a94a7ba"

LV_FONT_CONV_VERSION="1.5.3"

# Mismas opciones con que se generaron las fuentes anteriores: 4 bits por pixel
# y sin comprimir. Cuesta algo mas de flash (que sobra) y evita trabajo de
# decodificacion en cada frame.
LV_FONT_CONV_OPTS=(--no-compress --no-prefilter --bpp 4 --format lvgl --force-fast-kern-format)

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/firmware/main/fonts"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> Descargando y verificando los TTF"
fetch() {  # <url> <sha256> <destino>
    curl -fsSL "$1" -o "$WORK/$3"
    local real
    real="$(shasum -a 256 "$WORK/$3" | awk '{print $1}')"
    if [ "$real" != "$2" ]; then
        echo "ERROR: $3 no coincide." >&2
        echo "  esperado: $2" >&2
        echo "  obtenido: $real" >&2
        exit 1
    fi
    echo "    $3  OK"
}
fetch "$RAJDHANI_URL" "$RAJDHANI_SHA" "Rajdhani-SemiBold.ttf"
fetch "$POPPINS_URL" "$POPPINS_SHA" "Poppins-Regular.ttf"

echo "==> Instalando lv_font_conv $LV_FONT_CONV_VERSION"
# La cache de npm va dentro del temporal para no depender de ~/.npm
(cd "$WORK" && npm_config_cache="$WORK/npm-cache" npm install --silent --no-audit --no-fund \
    "lv_font_conv@$LV_FONT_CONV_VERSION")

echo "==> Generando fuentes en firmware/main/fonts/"
generate() {  # <ttf> <tamano> <nombre>
    "$WORK/node_modules/.bin/lv_font_conv" \
        "${LV_FONT_CONV_OPTS[@]}" --size "$2" --font "$WORK/$1" -r 0x20-0xFF \
        -o "$OUT/$3.c"
    printf "    %-22s %s px\n" "$3" "$2"
}
generate "Poppins-Regular.ttf"   16 font_poppins_16
generate "Rajdhani-SemiBold.ttf" 20 font_rajdhani_20
generate "Rajdhani-SemiBold.ttf" 28 font_rajdhani_28
generate "Rajdhani-SemiBold.ttf" 40 font_rajdhani_40
generate "Rajdhani-SemiBold.ttf" 64 font_rajdhani_64
generate "Rajdhani-SemiBold.ttf" 128 font_rajdhani_128

echo "==> Listo. Revisa el diff de firmware/main/fonts/ y recompila."
