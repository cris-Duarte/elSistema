#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-tetris-test.XXXXXX")
trap 'rm -f "$task_temp/tetris_test" "$task_temp/audio_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" \
    "$task_root/firmware/main/tetris_model.cpp" "$task_root/firmware/main/tetris_sound.cpp" \
    "$task_root/tools/tests/tetris_test.cpp" -o "$task_temp/tetris_test"
"$task_temp/tetris_test"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread \
    -I "$task_root/tools/tests/freertos_stubs" -I "$task_root/firmware/main" \
    "$task_root/firmware/main/audio_session.cpp" "$task_root/tools/tests/tetris_audio_test.cpp" \
    -o "$task_temp/audio_test"
"$task_temp/audio_test"
