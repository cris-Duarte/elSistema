#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-home-test.XXXXXX")
trap 'rm -f "$task_temp/home_scene_test" "$task_temp/device_state_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" \
    "$task_root/firmware/main/home_scene.cpp" "$task_root/tools/tests/home_scene_test.cpp" \
    -o "$task_temp/home_scene_test"
"$task_temp/home_scene_test"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" \
    "$task_root/tools/tests/device_state_test.cpp" -o "$task_temp/device_state_test"
"$task_temp/device_state_test"
