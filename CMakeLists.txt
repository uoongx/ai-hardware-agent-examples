cmake_minimum_required(VERSION 3.10)
project(ai_hardware_agent_examples VERSION 0.1.0 LANGUAGES C CXX)

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
#  Examples  (for ws63 platforms)
# ====================================================================

if(CONVAI_PLATFORM STREQUAL "ws63")
    add_subdirectory(examples/goldieos)
endif()

