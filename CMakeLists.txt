cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(pio_hub75)

pico_generate_pio_header(pio_hub75 ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)

target_sources(pio_hub75 PRIVATE 
	hub75.c
 	pixels.c
  	demos.c
   	demos/snek.c
    webusb_main.c
	usb_descriptors.c)

target_compile_definitions(pio_hub75 PRIVATE
	PICO_DEFAULT_UART_TX_PIN=28
	PICO_DEFAULT_UART_RX_PIN=29
)

target_link_libraries(pio_hub75 PRIVATE 
	pico_stdlib
	pico_bootrom
	pico_mem_ops
	pico_multicore
	pico_time
	hardware_pio
	hardware_resets
	hardware_irq
	tinyusb_device
	tinyusb_board
)
pico_add_extra_outputs(pio_hub75)

target_include_directories(pio_hub75 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

