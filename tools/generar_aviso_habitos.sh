#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/voz.aiff" "$task_tmp/voz.pcm"; rmdir "$task_tmp"' EXIT HUP INT TERM
# Local macOS voice; no cloud TTS, no habit names or user recordings.
say -v Paulina -r 165 -o "$task_tmp/voz.aiff" "Tienes un hábito pendiente. Revisa El Sistema"
ffmpeg -loglevel error -y -i "$task_tmp/voz.aiff" -ar 16000 -ac 1 -f s16le \
    "$task_tmp/voz.pcm"
test "$(wc -c < "$task_tmp/voz.pcm")" -ge 16000 || {
    echo 'La voz no produjo PCM. Ejecuta el script con acceso al servicio de voz de macOS.' >&2
    exit 1
}
mv "$task_tmp/voz.pcm" "$task_root/firmware/main/assets/habits_notice.pcm"
