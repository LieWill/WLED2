set(srcs "main.cpp" "../Src/ws2812.cpp"  "../Src/Dot_matrix_screen.cpp" "../Src/bluetooth.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "../Inc" ".")
