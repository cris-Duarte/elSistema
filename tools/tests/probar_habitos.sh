#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-habits-build.XXXXXX")
trap 'rm -f "$task_tmp/cjson.o" "$task_tmp/habits_test" "$task_tmp/repository.o" "$task_tmp/habits_storage_test"; rmdir "$task_tmp"' EXIT HUP INT TERM
task_cjson="${IDF_PATH:-/Users/cristhianduarte/.espressif/v5.4.2/esp-idf}/components/json/cJSON"
clang -O2 -c "$task_cjson/cJSON.c" -o "$task_tmp/cjson.o"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" -I "$task_cjson" \
    "$task_root/firmware/main/habit_model.cpp" "$task_root/firmware/main/habit_json.cpp" \
    "$task_root/firmware/main/habit_repository.cpp" "$task_root/tools/tests/habits_test.cpp" \
    "$task_tmp/cjson.o" -o "$task_tmp/habits_test"
"$task_tmp/habits_test"
# Also exercise the ESP_PLATFORM repository branch with the named partition
# already mounted, rather than letting desktop-only tests skip the lookup.
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -DESP_PLATFORM \
    -I "$task_root/tools/tests/stubs/habits_storage" -I "$task_root/firmware/main" \
    -c "$task_root/firmware/main/habit_repository.cpp" -o "$task_tmp/repository.o"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -I "$task_root/firmware/main" -I "$task_cjson" \
    "$task_root/firmware/main/habit_model.cpp" "$task_root/firmware/main/habit_json.cpp" \
    "$task_root/tools/tests/habits_test.cpp" "$task_tmp/repository.o" "$task_tmp/cjson.o" \
    -o "$task_tmp/habits_storage_test"
"$task_tmp/habits_storage_test"
