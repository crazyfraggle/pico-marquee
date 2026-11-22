cmake_minimum_required(VERSION 3.13)

# Prefer the pinned SDK submodule when no SDK path was supplied explicitly.
if (NOT DEFINED PICO_SDK_PATH AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/pico-sdk/pico_sdk_init.cmake")
	set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
endif()

# Default to a WiFi-capable board. Override with -DPICO_BOARD=pico for a
# plain (USB-only) Pico; the WiFi code is compiled out automatically when the
# selected board has no CYW43 chip.
if (NOT DEFINED PICO_BOARD)
	set(PICO_BOARD pico_w)
endif()

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
   	demos/matrix.c
   	demos/scroller.c
   	demos/morph.c
    text.c
    clock.c
    webusb_main.c
	wifi.c
	ha_mqtt.c
	usb_descriptors.c)

target_compile_definitions(pio_hub75 PRIVATE
	PICO_DEFAULT_UART_TX_PIN=16
	PICO_DEFAULT_UART_RX_PIN=17
)

# WiFi credentials and the TCP port the command listener binds to. Credentials
# may be supplied through the environment so they do not appear in shell
# history or the CMake command line.
if (NOT DEFINED WIFI_SSID)
	if (DEFINED ENV{WIFI_SSID})
		set(WIFI_SSID "$ENV{WIFI_SSID}")
	else()
		set(WIFI_SSID "")
	endif()
endif()
if (NOT DEFINED WIFI_PASSWORD)
	if (DEFINED ENV{WIFI_PASSWORD})
		set(WIFI_PASSWORD "$ENV{WIFI_PASSWORD}")
	else()
		set(WIFI_PASSWORD "")
	endif()
endif()
# Local time for the clock demo: a fixed offset from UTC in minutes, plus an
# hour during EU summer time when CLOCK_EU_DST is on. Defaults suit Norway.
if (NOT DEFINED CLOCK_UTC_OFFSET_MIN)
	set(CLOCK_UTC_OFFSET_MIN 60)
endif()
if (NOT DEFINED CLOCK_EU_DST)
	set(CLOCK_EU_DST 1)
endif()
if (NOT DEFINED NTP_SERVER)
	set(NTP_SERVER "pool.ntp.org")
endif()
if (NOT DEFINED MARQUEE_TCP_PORT)
	set(MARQUEE_TCP_PORT 4242)
endif()
# Home Assistant MQTT broker. Leaving MQTT_HOST empty compiles the
# integration out. Read from the environment like the WiFi credentials and
# never cached, so the password stays out of CMakeCache.txt.
foreach(var MQTT_HOST MQTT_USER MQTT_PASSWORD)
	if (NOT DEFINED ${var})
		if (DEFINED ENV{${var}})
			set(${var} "$ENV{${var}}")
		else()
			set(${var} "")
		endif()
	endif()
endforeach()
if (NOT DEFINED MQTT_PORT)
	if (DEFINED ENV{MQTT_PORT})
		set(MQTT_PORT "$ENV{MQTT_PORT}")
	else()
		set(MQTT_PORT 1883)
	endif()
endif()
if (NOT MQTT_HOST STREQUAL "")
	target_compile_definitions(pio_hub75 PRIVATE
		MARQUEE_MQTT=1
		MQTT_HOST=\"${MQTT_HOST}\"
		MQTT_PORT=${MQTT_PORT}
		MQTT_USER=\"${MQTT_USER}\"
		MQTT_PASSWORD=\"${MQTT_PASSWORD}\"
	)
endif()

target_compile_definitions(pio_hub75 PRIVATE
	WIFI_SSID=\"${WIFI_SSID}\"
	WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
	MARQUEE_TCP_PORT=${MARQUEE_TCP_PORT}
	CLOCK_UTC_OFFSET_MIN=${CLOCK_UTC_OFFSET_MIN}
	CLOCK_EU_DST=${CLOCK_EU_DST}
	NTP_SERVER=\"${NTP_SERVER}\"
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

# Link the CYW43 WiFi driver + lwIP stack only on boards that have the chip.
# PICO_CYW43_SUPPORTED is set by the SDK based on the selected PICO_BOARD.
# The "poll" arch variant runs all lwIP callbacks synchronously from the main
# loop (via cyw43_arch_poll), matching the existing cooperative task model and
# avoiding IRQ-context access to the shared framebuffer.
if (PICO_CYW43_SUPPORTED)
	target_compile_definitions(pio_hub75 PRIVATE PICO_CYW43_SUPPORTED=1)
	target_link_libraries(pio_hub75 PRIVATE pico_cyw43_arch_lwip_poll pico_lwip_sntp pico_lwip_mqtt pico_unique_id)
endif()

pico_add_extra_outputs(pio_hub75)

target_include_directories(pio_hub75 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
