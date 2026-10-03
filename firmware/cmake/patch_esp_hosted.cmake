function(el_sistema_replace_or_verify FILE OLD NEW MARKER)
    if(NOT EXISTS "${FILE}")
        message(FATAL_ERROR "No existe el archivo ESP-Hosted esperado: ${FILE}")
    endif()
    file(READ "${FILE}" CONTENT)
    # Comprueba primero el resultado. Algunos patrones originales (por ejemplo,
    # una llamada seguida de una declaración) también son subcadenas del texto
    # parcheado y volver a reemplazarlos duplicaría el parche.
    string(FIND "${CONTENT}" "${MARKER}" NEW_POS)
    if(NOT NEW_POS EQUAL -1)
        return()
    endif()
    string(FIND "${CONTENT}" "${OLD}" OLD_POS)
    if(NOT OLD_POS EQUAL -1)
        string(REPLACE "${OLD}" "${NEW}" CONTENT "${CONTENT}")
        file(WRITE "${FILE}" "${CONTENT}")
        message(STATUS "El Sistema: parche de tolerancia aplicado a ${FILE}")
    else()
        message(FATAL_ERROR
            "ESP-Hosted cambió y el parche seguro ya no coincide: ${FILE}. "
            "Revisa la versión fijada antes de compilar.")
    endif()
endfunction()

function(el_sistema_patch_esp_hosted HOSTED_DIR)
    set(HOST_INIT "${HOSTED_DIR}/host/esp_hosted_host_init.c")
    set(HOST_API "${HOSTED_DIR}/host/api/src/esp_hosted_api.c")

    # El componente original inicializa SDIO desde un constructor anterior a
    # app_main. Se difiere hasta que el usuario solicita Wi-Fi, de modo que una
    # incompatibilidad del C6 no impida usar las funciones locales.
    el_sistema_replace_or_verify("${HOST_INIT}"
        "ESP_ERROR_CHECK(esp_hosted_init());"
        "/* El Sistema: inicializacion diferida hasta ConnectivityService. */\n\tESP_LOGI(TAG, \"ESP-Hosted initialization deferred\");"
        "ESP-Hosted initialization deferred")

    # La API 1.4.1 aborta con ESP_ERROR_CHECK. En las rutas de inicializacion
    # convertimos esos abortos en errores retornables para mostrarlos en la UI.
    el_sistema_replace_or_verify("${HOST_API}"
        "set_host_modules_log_level();"
        "set_host_modules_log_level();\n\tesp_err_t init_err = ESP_OK;"
        "esp_err_t init_err = ESP_OK")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(esp_hosted_set_default_config());"
        "if ((init_err = esp_hosted_set_default_config()) != ESP_OK) return init_err;"
        "init_err = esp_hosted_set_default_config")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(transport_drv_init(transport_active_cb));"
        "if ((init_err = transport_drv_init(transport_active_cb)) != ESP_OK) return init_err;"
        "init_err = transport_drv_init")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(add_esp_wifi_remote_channels());"
        "if ((init_err = add_esp_wifi_remote_channels()) != ESP_OK) return init_err;"
        "init_err = add_esp_wifi_remote_channels")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(rpc_init());"
        "if ((init_err = rpc_init()) != ESP_OK) return init_err;"
        "init_err = rpc_init")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(transport_drv_reconfigure());"
        "if (transport_drv_reconfigure() != ESP_OK) return ESP_FAIL;"
        "transport_drv_reconfigure() != ESP_OK")
    el_sistema_replace_or_verify("${HOST_API}"
        "ESP_ERROR_CHECK(esp_hosted_reconfigure());"
        "if (esp_hosted_reconfigure() != ESP_OK) return ESP_FAIL;"
        "esp_hosted_reconfigure() != ESP_OK")
endfunction()
