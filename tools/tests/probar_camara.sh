#!/bin/sh
set -eu
task_root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
task_temp=$(mktemp -d "${TMPDIR:-/tmp}/elsistema-camera-test.XXXXXX")
trap 'rm -f "$task_temp/camera_test" "$task_temp/pixel_test"; rmdir "$task_temp"' EXIT HUP INT TERM
clang++ -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -I "$task_root/firmware/main" "$task_root/firmware/main/camera_model.cpp" \
    "$task_root/tools/tests/camera_test.cpp" -o "$task_temp/camera_test"
"$task_temp/camera_test"
task_idf_root=${IDF_PATH:-"$HOME/.espressif/v5.4.2/esp-idf"}
if [ -f "$task_idf_root/components/esp_driver_jpeg/include/driver/jpeg_types.h" ]; then
    clang++ -std=c++17 -O2 -Wall -Wextra -Werror \
        -I "$task_root/firmware/main" \
        -I "$task_idf_root/components/esp_driver_jpeg/include" \
        -I "$task_idf_root/components/hal/include" \
        "$task_root/tools/tests/camera_pixel_format_test.cpp" -o "$task_temp/pixel_test"
    "$task_temp/pixel_test"
else
    printf '%s\n' 'SKIP: contrato de color requiere IDF_PATH con ESP-IDF 5.4.2; las pruebas portables sí se ejecutaron.'
fi
