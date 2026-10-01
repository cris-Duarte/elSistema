cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET esp32p4)

# Con -DEL_SISTEMA_TESTS=ON se compila el componente de pruebas de dominio y el
# firmware las ejecuta al arrancar, antes de tocar hardware o almacenamiento.
option(EL_SISTEMA_TESTS "Compilar y ejecutar las pruebas de dominio al arrancar" OFF)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(EL_SISTEMA_TESTS)
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/main/test")
endif()

set(PROJECT_VER "0.2.0")
project(el_sistema)
