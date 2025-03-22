# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Inicializa o Pico SDK
include(pico_sdk_import.cmake)

set(PICO_BOARD pico CACHE STRING "Board type")

project(ProjetoRobo C CXX ASM)

# Inicializa o Raspberry Pi Pico SDK
pico_sdk_init()

# Adiciona o executável
add_executable(ProjetoRobo ProjetoRobo.c inc/ssd1306_i2c.c)

pico_set_program_name(ProjetoRobo "ProjetoRobo")
pico_set_program_version(ProjetoRobo "0.1")

# Gera o cabeçalho PIO
pico_generate_pio_header(ProjetoRobo ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

# Habilita/desabilita saída via UART/USB
pico_enable_stdio_uart(ProjetoRobo 0)
pico_enable_stdio_usb(ProjetoRobo 1)

# Adiciona bibliotecas padrão ao build
target_link_libraries(ProjetoRobo
        pico_stdlib
        hardware_pio
        hardware_clocks
        hardware_i2c
        hardware_pwm)

# Adiciona diretórios de inclusão
target_include_directories(ProjetoRobo PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/inc
)

pico_add_extra_outputs(ProjetoRobo)