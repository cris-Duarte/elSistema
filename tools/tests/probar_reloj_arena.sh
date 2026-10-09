#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-hourglass-test.XXXXXX")
trap 'rm -f "$task_temp/hourglass_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" \
    "$task_root/firmware/main/hourglass_model.cpp" "$task_root/firmware/main/hourglass_raster.cpp" \
    "$task_root/tools/tests/hourglass_test.cpp" -o "$task_temp/hourglass_test"
"$task_temp/hourglass_test" "$@"
