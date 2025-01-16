add_library(modbus_tcp_poll
    modbus_tcp_poll.c
)

target_include_directories(modbus_tcp_poll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(modbus_tcp_poll PUBLIC
    system
    logging
    modbus_tcp_client
    modbus_tcp_server
    huawei_modbus_converter
    solar_logger
)
