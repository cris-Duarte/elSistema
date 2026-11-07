option(VOXEL_DIAGNOSTIC "Arrancar con patron DSI" OFF)
option(VOXEL_QUALITY "Render 256x144 (requiere medir rendimiento)" OFF)
option(VOXEL_BENCHMARK "Prueba autonoma de cuatro escenas" OFF)
if(VOXEL_BENCHMARK AND VOXEL_DIAGNOSTIC)
    message(FATAL_ERROR "Elige VOXEL_BENCHMARK o VOXEL_DIAGNOSTIC, no ambos")
endif()
idf_component_register(
    SRCS "../src/main.cpp" "../src/World.cpp" "../src/Terrain.cpp"
         "../src/Camera.cpp" "../src/Raycast.cpp" "../src/Renderer.cpp"
         "../src/Player.cpp" "../src/Input.cpp" "../src/Game.cpp"
         "../src/Scale.cpp" "../src/DisplayBackend.cpp" "../src/Hud.cpp"
         "../src/Diagnostics.cpp" "../src/Benchmark.cpp"
    INCLUDE_DIRS "../include"
    REQUIRES M5Unified M5GFX esp_lcd esp_timer esp_mm heap)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
if(VOXEL_DIAGNOSTIC)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE VOXEL_DIAGNOSTIC=1)
endif()
if(VOXEL_QUALITY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE VOXEL_WIDTH=256 VOXEL_HEIGHT=144)
endif()
if(VOXEL_BENCHMARK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE VOXEL_BENCHMARK=1)
endif()
