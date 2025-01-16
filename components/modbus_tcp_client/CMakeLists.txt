add_library(modbus_tcp_client
    modbus_tcp_client.c
)

target_include_directories(modbus_tcp_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(modbus_tcp_client PUBLIC
    system
    logging
)
