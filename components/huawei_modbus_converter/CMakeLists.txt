add_library(huawei_modbus_converter
    huawei_modbus_converter.c
)

target_include_directories(huawei_modbus_converter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(huawei_modbus_converter PUBLIC
    logging
    modbus_tcp_poll
    m # math.h
)
