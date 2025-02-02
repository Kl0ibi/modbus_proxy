add_library(can_ez3_modbus_converter
    can_ez3_modbus_converter.c
)

target_include_directories(can_ez3_modbus_converter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(can_ez3_modbus_converter PUBLIC
    system
    logging
    modbus_tcp_poll
)
