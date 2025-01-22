add_library(http_server
    http_server.c
)

target_include_directories(http_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(http_server PUBLIC
    system
    logging
    modbus_tcp_poll
    huawei_modbus_converter
    nrgkick_modbus_converter
)
