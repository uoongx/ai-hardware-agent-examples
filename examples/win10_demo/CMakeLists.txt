# ====================================================================
#  Windows 10 demo application
# ====================================================================

add_executable(win10_demo main.c)

target_link_libraries(win10_demo PRIVATE convai_sdk)
