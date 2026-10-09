cmake_minimum_required(VERSION 3.16)
project(tetris_lvgl_smoke LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(TASK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LV_CONF_SKIP ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
add_subdirectory("${TASK_ROOT}/firmware/components/lvgl" lvgl)
target_compile_definitions(lvgl PUBLIC LV_CONF_SKIP=1 LV_USE_OS=0 LV_MEM_SIZE=1048576 LV_LVGL_H_INCLUDE_SIMPLE=1)
add_executable(tetris_lvgl_smoke main.cpp
    "${TASK_ROOT}/firmware/main/tetris_model.cpp"
    "${TASK_ROOT}/firmware/main/tetris_renderer.cpp"
    "${TASK_ROOT}/firmware/main/fonts/font_rajdhani_20.c"
    "${TASK_ROOT}/firmware/main/fonts/font_rajdhani_28.c")
target_include_directories(tetris_lvgl_smoke PRIVATE "${TASK_ROOT}/firmware/main")
target_link_libraries(tetris_lvgl_smoke PRIVATE lvgl)
