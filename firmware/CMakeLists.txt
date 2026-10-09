cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32p4)
# Algunos componentes 1.x seleccionan sus fragmentos Kconfig a partir de esta
# variable de entorno. EIM no la exporta al invocar CMake directamente.
set(ENV{ESP_IDF_VERSION} "5.4")

# Con -DEL_SISTEMA_TESTS=ON se compila el componente de pruebas de dominio y el
# firmware las ejecuta al arrancar, antes de tocar hardware o almacenamiento.
option(EL_SISTEMA_TESTS "Compilar y ejecutar las pruebas de dominio al arrancar" OFF)
option(EL_SISTEMA_RADAR_SMOKE "Abrir Radar y verificar escaneo real; solo con TESTS" OFF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(EL_SISTEMA_TESTS)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/main/test")
endif()

set(PROJECT_VER "0.4.2")
project(el_sistema)

# esp_hosted 1.4.1 se fija por compatibilidad con el C6 de fabrica, pero su
# inicializador aborta antes de app_main ante ciertos fallos. Aplicamos un parche
# reproducible al componente descargado para que esos errores lleguen a la UI.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/patch_esp_hosted.cmake")
el_sistema_patch_esp_hosted("${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp_hosted")
