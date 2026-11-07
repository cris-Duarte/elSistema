idf_component_register(SRCS "app_main.cpp" "hardware.cpp" "m5_platform.cpp" "services.cpp" "ui.cpp" "assets/logo.c" "fonts/poppins_18.c" "fonts/poppins_20.c" "fonts/poppins_24.c" "fonts/poppins_28.c" "fonts/poppins_32.c" "fonts/poppins_40.c" "fonts/poppins_48.c"
 INCLUDE_DIRS "." "assets"
 REQUIRES m5stack_tab5 lvgl esp_timer nvs_flash esp_wifi esp_netif esp_event json m5stack__m5unified m5stack__m5gfx espressif__esp_hosted espressif__esp_wifi_remote)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=i2c_master_transmit" "-Wl,--wrap=i2c_master_receive" "-Wl,--wrap=i2c_master_transmit_receive" "-Wl,--wrap=i2c_master_probe" "-Wl,--wrap=i2c_master_multi_buffer_transmit")
