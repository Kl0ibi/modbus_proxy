add_library(solar_logger
    solar_logger.c
)

target_include_directories(solar_logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(solar_logger PUBLIC
    system
    logging
    db_handler
    huawei_modbus_converter
    nrgkick_modbus_converter
    can_ez3_modbus_converter
)
