add_library(nrgkick_modbus_converter
    nrgkick_modbus_converter.c
)

target_include_directories(nrgkick_modbus_converter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nrgkick_modbus_converter PUBLIC
    system
    logging
    modbus_tcp_poll
)
