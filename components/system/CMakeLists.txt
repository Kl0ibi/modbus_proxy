add_library(system
    system.c
)

target_include_directories(system PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
