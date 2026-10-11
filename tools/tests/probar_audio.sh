#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-audio-test.XXXXXX")
trap 'rm -f "$task_temp/playback_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror \
    -I "$task_root/tools/tests/stubs/audio" -I "$task_root/tools/tests/freertos_stubs" \
    -I "$task_root/firmware/main" "$task_root/firmware/main/audio.cpp" \
    "$task_root/tools/tests/audio_playback_test.cpp" -o "$task_temp/playback_test"
"$task_temp/playback_test"
