add_library(modbus_tcp_server
    modbus_tcp_server.c
)

target_include_directories(modbus_tcp_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(modbus_tcp_server PUBLIC
    logging
    modbus_tcp_poll
)
