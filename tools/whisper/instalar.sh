#!/usr/bin/env bash
#
# Instala whisper.cpp v1.9.4 con Metal y descarga ggml-small.bin (multilingüe).
#
# Uso:  tools/whisper/instalar.sh [--dir ~/whisper.cpp]
#
# - Clona el tag v1.9.4 (no master) para fijar la versión del plan v0.4.0.
# - Compila con CMake (Metal activo por defecto en macOS).
# - Descarga el modelo `small` multilingüe, NO `small.en` (solo inglés).
# - Verifica el SHA esperado antes de dar por buena la instalación.
#
set -euo pipefail

VERSION="v1.9.4"
MODEL="small"
MODEL_SHA1="55356645c2b361a969dfd0ef2c5a50d530afd8d5"
DIR="${1:-${WHISPER_DIR:-$HOME/whisper.cpp}}"
if [[ "${1:-}" == "--dir" ]]; then DIR="${2:?falta directorio}"; fi

echo "==> whisper.cpp $VERSION en $DIR"
if [[ ! -d "$DIR/.git" ]]; then
    git clone --depth 1 --branch "$VERSION" https://github.com/ggml-org/whisper.cpp.git "$DIR"
else
    git -C "$DIR" fetch --tags --depth 1 origin "refs/tags/$VERSION:refs/tags/$VERSION" || true
    git -C "$DIR" checkout "$VERSION"
fi

echo "==> Compilando con Metal (CMake)"
cmake -S "$DIR" -B "$DIR/build" -DGGML_METAL=ON -DBUILD_SHARED_LIBS=OFF
cmake --build "$DIR/build" -j --config Release --target whisper-server whisper-cli

echo "==> Descargando modelo ggml-$MODEL.bin"
"$DIR/models/download-ggml-model.sh" "$MODEL" "$DIR/models"

echo "==> Verificando SHA del modelo"
real="$(shasum -a 1 "$DIR/models/ggml-$MODEL.bin" | awk '{print $1}')"
if [[ "$real" != "$MODEL_SHA1" ]]; then
    echo "ERROR: SHA del modelo no coincide." >&2
    echo "  esperado: $MODEL_SHA1" >&2
    echo "  obtenido: $real" >&2
    exit 1
fi

echo "==> Listo:"
echo "    servidor: $DIR/build/bin/whisper-server"
echo "    modelo:   $DIR/models/ggml-$MODEL.bin"
echo "Arranca con: tools/whisper/servidor.sh --dir $DIR"
