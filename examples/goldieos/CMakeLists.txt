# ====================================================================
#  goldieos WS63 — embedded RISC-V firmware
#
#  Cross-compiles for HiSilicon WS63 (RISC-V 32-bit + FPU, LiteOS, lwIP).
#  Outputs goldieos.fwpkg for board flashing.
# ====================================================================

cmake_minimum_required(VERSION 3.10)

# ---- Toolchain ——
# Specify via command line, matching old project conventions:
#   cmake -G "MinGW Makefiles" -DCONVAI_PLATFORM=ws63 \
#         -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc \
#         -DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++ \
#         -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 \
#         -DCMAKE_MAKE_PROGRAM=make ..
# Add the toolchain bin directory to PATH before invoking cmake.

# ---- Header search paths ----

set(WS63_INCLUDE_DIRS
    # SDK public headers
    ${CMAKE_SOURCE_DIR}/include

    # SDK integration (replaces cloud_service)
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/compat

    # Framework core
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core

    # OS abstraction layer
    ${CMAKE_CURRENT_SOURCE_DIR}/include/osal

    # Service interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/alarm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/ntp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/audio

    # System resources
    ${CMAKE_CURRENT_SOURCE_DIR}/include/system_res

    # Platform headers (WS63 hardware)
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/sle
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/ble
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/common
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/slp

    # Third-party headers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/lwip
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/mbedtls
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs-R0.11/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webrtc_vad
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/helix/real
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/helix/pub
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus-1.6.1/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speex_aec
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speex_aec/include

    # GUI headers
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gui

    # Driver headers
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/codec/es8311_drv
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/lcd
)

# ---- Source files ----

# Init (system start-up)
set(WS63_INIT_SRC
    init/system_init.c
)

# Platform sources
file(GLOB WS63_PLATFORM_SRC
    platform/ws63/*.c
    platform/ws63/adc/*.c
    platform/ws63/sle/*.c
    platform/ws63/stubs.c
)
# ConvAI platform implementation
list(APPEND WS63_PLATFORM_SRC
    platform/convai_platform_ws63.c
)

# Driver sources
set(WS63_DRIVER_SRC
    drivers/lcd/st7789.c
    drivers/touch/cst816d.c
    drivers/codec/es8311_drv/es8311.c
    drivers/keyboard/gpio_keyboard.c
    drivers/i2c/i2c.c
    drivers/rtc/pcf8563.c
    drivers/battery/bat_driver.c
    drivers/extern_io/aw9523b.c
    drivers/audio_pa/pa_drv.c
)

# Service sources
file(GLOB WS63_SERVICE_SRC
    services/alarm_service/*.c
    services/ntp_service/*.c
    services/sle_service/*.c
    # services/network_components/*.c
    services/aud_algo/*.c
)

# SDK integration (replaces cloud_service)
file(GLOB WS63_SDK_INTEGRATION_SRC
    sdk_integration/*.c
)

# App sources — WS63 embedded apps
file(GLOB WS63_APP_SRC
    apps/launcher/*.cpp
    apps/settings/*.cpp
    # apps/alarm/*.cpp
    apps/animaton_player/*.cpp
    # apps/recorder/*.cpp
    # apps/AItalk/*.cpp
    # apps/Walkie-talkie/*.cpp
    # apps/Story/*.cpp
    # apps/Story/*.c
    apps/shut_down/*.cpp
    apps/charging_only/*.cpp
)

# App-specific include dirs (app source + assets)
set(WS63_APP_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/launcher/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/animaton_player
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/animaton_player/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/recorder
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/recorder/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/AItalk
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/AItalk/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/settings/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/alarm
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/alarm/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/shut_down
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/shut_down/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/charging_only
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/charging_only/assets
)

# Third-party sources (compiled from source)
file(GLOB WS63_THIRDPARTY_SRC
    third_party/speex_aec/libspeexdsp/*.c
    third_party/fatfs-R0.11/src/*.c
    third_party/fatfs-R0.11/src/option/*.c
    third_party/webrtc_vad/*.c
    third_party/webrtc_vad/signal_processing/*.c
    third_party/webrtc_vad/vad/*.c
    third_party/helix/*.c
    third_party/helix/real/*.c
)

# ---- Opus codec ----
set(OPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus-1.6.1)
if(EXISTS ${OPUS_DIR}/CMakeLists.txt)
    add_compile_definitions(CUSTOM_MODES)
    set(OPUS_VAR_ARRAYS OFF CACHE BOOL "Disable variable length arrays" FORCE)
    set(OPUS_USE_ALLOCA OFF CACHE BOOL "Disable alloca" FORCE)

    set(OPUS_FIXED_POINT ON CACHE BOOL "Enable fixed-point" FORCE)
    set(OPUS_ASM OFF CACHE BOOL "Disable assembly" FORCE)
    set(OPUS_INTRINSICS OFF CACHE BOOL "Disable intrinsics" FORCE)
    set(OPUS_DISABLE_FLOAT_API ON CACHE BOOL "Disable float API" FORCE)  # 纯定点可关闭浮点API
    # 可选：OPUS_CUSTOM_MODES ON 如果需自定义编码模式
    set(OPUS_DISABLE_FLOAT_API ON CACHE BOOL "Disable float API" FORCE)
    set(OPUS_ENABLE_FLOAT_API OFF CACHE BOOL "Enable float API" FORCE)
    set(OPUS_CUSTOM_MODES ON CACHE BOOL "Enable custom modes" FORCE)    
    set(OPUS_DRED OFF CACHE BOOL "Disable DRED" FORCE)                   # 关闭丢包隐藏（如果不需要）
    set(OPUS_OSCE OFF CACHE BOOL "Disable OSCE" FORCE)                   # 关闭声道耦合增强
    set(OPUS_FORTIFY_SOURCE OFF CACHE BOOL "Disable fortify" FORCE)      # 关闭安全检查（谨慎）	
    # 添加 Opus 子目录（构建后将生成 opus 静态库）
    add_subdirectory(${OPUS_DIR} opus_build)
endif()

# ---- Compile definitions (WS63 hardware) ----
add_compile_definitions(
    PLATFORM_TYPE_WS63
    ST7789_SPI_LCD
    CONFIG_SUPPORT_ES8311_CODEC
    SUPPORT_EXT_GPIO_PA
    SUPPORT_SLE
    __EMBEDDED__
    HAVE_CONFIG_H
    FIXED_POINT
    USE_KISS_FFT

    # Hardware features
    SUPPORT_BATTERY
    SUPPORT_GPIO_KEYBOARD
    LCM_USE_EXT_GPIO
    SUPPORT_CST816D_TOUCH
    USE_EXT_ASR_CHIP_AC2817
    SUPPORT_PCF8563_RTC
    DRV_CORE

    CONFIG_ROM_COMPILE
    INLINE_TO_FORCEINLINE
    LWIP_CONFIG_FILE="lwipopts_default.h"
)

# ---- C Compile options (RISC-V specific) ----
add_compile_options(
	-w
	-march=rv32imfc
	-mabi=ilp32f
	# --short-enums
	-std=gnu99
    -fasm
    -fcommon
    -Wall -Wextra
    -Winit-self -Wpointer-arith -Wstrict-prototypes
	-fno-strict-aliasing
	-Os
	-fno-unwind-tables
	-ffreestanding
    -fdata-sections -ffunction-sections
    -nostdlib -pipe
	-fno-tree-scev-cprop
    -mpush-pop -msmall-data-limit=0 -fno-ipa-ra
    -g
)

# ---- Link options ----
add_link_options(
    -static
    -Wl,--enjal16
    -nostdlib
)

# ---- Precompiled SDK library ----
add_library(convai_sdk STATIC IMPORTED)
set_target_properties(convai_sdk PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a
)

# ---- Library target ----
add_library(goldieos_ws63 STATIC
    ${WS63_INIT_SRC}
    ${WS63_PLATFORM_SRC}
    ${WS63_DRIVER_SRC}
    ${WS63_SERVICE_SRC}
    ${WS63_SDK_INTEGRATION_SRC}
    ${WS63_APP_SRC}
    ${WS63_THIRDPARTY_SRC}
)

target_include_directories(goldieos_ws63 PRIVATE
    ${WS63_INCLUDE_DIRS}
    ${WS63_APP_INCLUDE_DIRS}
)

target_link_libraries(goldieos_ws63 PRIVATE
    opus
    convai_sdk
    -lc  # Link musl C library for printf/vprintf
)

# ---- Board package path ----
# Priority: CMake argument > environment variable > fallback to relative path
if(WS63_BOARD_ROOT)
    set(WS63_LINK_ROOT "${WS63_BOARD_ROOT}")
elseif(DEFINED ENV{GOLDIEOS_ROOT})
    set(WS63_LINK_ROOT "$ENV{GOLDIEOS_ROOT}")
else()
    # Fallback: relative path from ws63/CMakeLists.txt
    # ws63/CMakeLists.txt -> examples/ -> aiHard/ -> koophone/ -> godie/goldie-os_pd001
    set(WS63_LINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Validate path exists
if(NOT EXISTS "${WS63_LINK_ROOT}")
    message(FATAL_ERROR "WS63 board package not found at: ${WS63_LINK_ROOT}\n"
        "Set WS63_BOARD_ROOT cmake variable or GOLDIEOS_ROOT environment variable.")
endif()

# ---- Linker step (ws63_link_v4.exe) ----
set(WS63_LINKER    "${WS63_LINK_ROOT}/tools/build/tools/ws63_link_v4.exe")
set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
set(OUTPUT_BIN "${OUTPUT_DIR}/goldieos.bin")
set(OUTPUT_ELF "${OUTPUT_DIR}/goldieos.elf")

# Library paths (relative, for ws63_link_v4.exe)
set(WS63_LIB_GOLDIEOS "libgoldieos_ws63.a")
set(WS63_LIB_OPUS "opus_build/libopus.a")
set(WS63_LIB_SDK "${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a")

add_custom_command(
    OUTPUT ${OUTPUT_BIN} ${OUTPUT_ELF}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
    COMMAND ${WS63_LINKER} ${WS63_LINK_ROOT} "${WS63_LIB_GOLDIEOS},${WS63_LIB_OPUS},${WS63_LIB_SDK}" ${OUTPUT_DIR}
    DEPENDS goldieos_ws63 opus convai_sdk
    COMMENT "Generating firmware with ws63_link_v4"
    VERBATIM
)

add_custom_target(ws63_firmware ALL
    DEPENDS ${OUTPUT_BIN}
    COMMENT "Firmware: ${OUTPUT_BIN}"
)

message(STATUS "GoldieOS WS63 configured. Compiler: ${CMAKE_C_COMPILER}")