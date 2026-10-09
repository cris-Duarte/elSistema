#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-radar-test.XXXXXX")
trap 'rm -f "$task_temp/radar_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" \
    "$task_root/firmware/main/radar_model.cpp" "$task_root/firmware/main/radar_raster.cpp" \
    "$task_root/tools/tests/radar_test.cpp" -o "$task_temp/radar_test"
"$task_temp/radar_test" "$@"
