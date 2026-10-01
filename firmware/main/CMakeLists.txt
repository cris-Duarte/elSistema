idf_component_register(
    SRCS
        "app_main.cpp"
        "fonts/font_es_14.c"
        "fonts/font_es_16.c"
        "fonts/font_es_20.c"
        "fonts/font_es_28.c"
    INCLUDE_DIRS "."
    REQUIRES m5stack_tab5 esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
