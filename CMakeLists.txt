cmake_minimum_required(VERSION 3.10)
project(convai_sdk VERSION 0.1.0 LANGUAGES C)

# ====================================================================
#  Global settings
# ====================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable compile warnings
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ====================================================================
#  Platform detection
# ====================================================================

if(WIN32)
    set(CONVAI_PLATFORM "win10" CACHE STRING "Target platform (linux/win10/ws63)")
else()
    set(CONVAI_PLATFORM "linux" CACHE STRING "Target platform (linux/win10/ws63)")
endif()

set(CONVAI_PLATFORM_SRC "" CACHE INTERNAL "")

if(CONVAI_PLATFORM STREQUAL "linux")
    set(CONVAI_PLATFORM_SRC platform/linux/convai_hal_linux.c)
    message(STATUS "ConvAI platform: Linux")
elseif(CONVAI_PLATFORM STREQUAL "win10")
    set(CONVAI_PLATFORM_SRC platform/win10/convai_hal_win.c)
    message(STATUS "ConvAI platform: Windows 10")
elseif(CONVAI_PLATFORM STREQUAL "ws63")
    set(CONVAI_PLATFORM_SRC platform/ws63/convai_hal_ws63.c)
    message(STATUS "ConvAI platform: WS63 (stub)")
else()
    message(FATAL_ERROR "Unknown CONVAI_PLATFORM '${CONVAI_PLATFORM}'")
endif()

# ====================================================================
#  Examples  (only for desktop platforms)
# ====================================================================

if(CONVAI_PLATFORM STREQUAL "linux")
    add_subdirectory(examples/linux_demo)
elseif(CONVAI_PLATFORM STREQUAL "win10")
    add_subdirectory(examples/win10_demo)
    add_subdirectory(examples/goldieos)
elseif(CONVAI_PLATFORM STREQUAL "ws63")
    add_subdirectory(examples/goldieos)
endif()

# ====================================================================
#  Tests  (opt-in, requires CONVAI_BUILD_TESTS=ON)
# ====================================================================

option(CONVAI_BUILD_TESTS "Build tests" OFF)
if(CONVAI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ====================================================================
#  mbedTLS support (opt-in)
# ====================================================================

option(CONVAI_USE_MBEDTLS "Enable TLS support via mbedTLS" OFF)

# TODO: add WS63 cross-compilation example when toolchain is available
