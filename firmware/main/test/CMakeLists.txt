# WHOLE_ARCHIVE es obligatorio: las pruebas solo aportan constructores de registro
# (TEST_CASE) que nadie referencia, y sin esto el enlazador no extrae el objeto de
# la libreria estatica, por lo que el runner veria cero pruebas.
idf_component_register(
    SRCS "test_domain.cpp"
    INCLUDE_DIRS ".."
    REQUIRES unity main
    WHOLE_ARCHIVE
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-field-initializers)
