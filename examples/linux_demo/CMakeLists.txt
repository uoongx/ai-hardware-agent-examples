# ====================================================================
#  Linux demo application
# ====================================================================

add_executable(linux_demo main.c)

target_link_libraries(linux_demo PRIVATE convai_sdk)

# The demo may also need pthread on some toolchains
if(UNIX)
    target_link_libraries(linux_demo PRIVATE pthread)
endif()
