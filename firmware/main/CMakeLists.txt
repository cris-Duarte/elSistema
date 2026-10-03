# "unity" debe figurar siempre: app_main.cpp incluye su cabecera cuando se compila
# con -DEL_SISTEMA_TESTS=ON, y la expansion de requisitos de IDF corre antes de que
# esa opcion sea visible. Es PRIV_REQUIRES porque solo lo usa este componente.
idf_component_register(
    SRCS
        "app_main.cpp"
        "assistant.cpp"
        "assistant_voice.cpp"
        "audio.cpp"
        "audio_session.cpp"
        "ble.cpp"
        "connectivity.cpp"
        "dictation.cpp"
        "hardware.cpp"
        "model.cpp"
        "repository.cpp"
        "services.cpp"
        "transcription.cpp"
        "ui.cpp"
        "assets/assistant_character.c"
        "assets/assistant_character_data.c"
        "fonts/font_poppins_16.c"
        "fonts/font_rajdhani_20.c"
        "fonts/font_rajdhani_28.c"
        "fonts/font_rajdhani_40.c"
        "fonts/font_rajdhani_64.c"
        "fonts/font_rajdhani_128.c"
    INCLUDE_DIRS "." "assets"
    REQUIRES m5stack_tab5 esp_timer json nvs_flash spiffs espressif__bmi270_sensor
             esp_event esp_http_client esp_netif esp_wifi espressif__esp_hosted espressif__esp_wifi_remote heap
             espressif__mdns bt
    PRIV_REQUIRES unity
    EMBED_FILES "assets/assistant_ack.pcm"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)

if(EL_SISTEMA_TESTS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE EL_SISTEMA_RUN_TESTS=1)
endif()
