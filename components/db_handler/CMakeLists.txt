add_library(db_handler
    db_handler.c
)

target_include_directories(db_handler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(db_handler PUBLIC
    system
    logging
    huawei_modbus_converter
)
