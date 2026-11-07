#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-retired-test.XXXXXX")
trap 'rm -f "$task_tmp/retired_storage_test"; rmdir "$task_tmp"' EXIT HUP INT TERM
clang++ -std=c++17 -O2 -Wall -Wextra -Werror \
    -I "$task_root/tools/tests/stubs/retired_storage" -I "$task_root/firmware/main" \
    "$task_root/firmware/main/retired_storage.cpp" "$task_root/tools/tests/retired_storage_test.cpp" \
    -o "$task_tmp/retired_storage_test"
"$task_tmp/retired_storage_test"
