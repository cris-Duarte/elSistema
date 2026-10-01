# "unity" debe figurar siempre: app_main.cpp incluye su cabecera cuando se compila
# con -DEL_SISTEMA_TESTS=ON, y la expansion de requisitos de IDF corre antes de que
# esa opcion sea visible. Es PRIV_REQUIRES porque solo lo usa este componente.
idf_component_register(
    SRCS
        "app_main.cpp"
        "hardware.cpp"
        "model.cpp"
        "repository.cpp"
        "services.cpp"
        "ui.cpp"
        "fonts/font_es_14.c"
        "fonts/font_es_16.c"
        "fonts/font_es_20.c"
        "fonts/font_es_28.c"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5 esp_timer json nvs_flash spiffs espressif__bmi270_sensor
    PRIV_REQUIRES unity
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

if(EL_SISTEMA_TESTS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE EL_SISTEMA_RUN_TESTS=1)
endif()
