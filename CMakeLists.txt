# Specify the minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(modbus_proxy)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the components
add_subdirectory(components/system)
add_subdirectory(components/logging)
add_subdirectory(components/modbus_tcp_client)
add_subdirectory(components/modbus_tcp_server)
add_subdirectory(components/modbus_tcp_poll)
add_subdirectory(components/huawei_modbus_converter)
add_subdirectory(components/nrgkick_modbus_converter)
add_subdirectory(components/can_ez3_modbus_converter)
add_subdirectory(components/db_handler)
add_subdirectory(components/http_server)
add_subdirectory(components/solar_logger)
add_subdirectory(components/cJSON)
add_subdirectory(components/fronius_solar_api)
add_subdirectory(main)

# Define the executable and link all components
add_executable(modbus_proxy
    main/main.c
)

# Link libraries to the executable
target_link_libraries(modbus_proxy
    system
    modbus_tcp_client
    modbus_tcp_server
    logging
    huawei_modbus_converter
    nrgkick_modbus_converter
    db_handler
    http_server
    solar_logger
    cJSON
    fronius_solar_api
)

# Include directories for the main target
target_include_directories(modbus_proxy PUBLIC
    ${CMAKE_SOURCE_DIR}/main/include
    ${CMAKE_SOURCE_DIR}/components/system/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_client/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_server/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_poll/include
    ${CMAKE_SOURCE_DIR}/components/logging/include
    ${CMAKE_SOURCE_DIR}/components/huawei_modbus_converter/include
    ${CMAKE_SOURCE_DIR}/components/nrgkick_modbus_converter/include
    ${CMAKE_SOURCE_DIR}/components/can_ez3_modbus_converter/include
    ${CMAKE_SOURCE_DIR}/components/db_handler/include
    ${CMAKE_SOURCE_DIR}/components/http_server/include
    ${CMAKE_SOURCE_DIR}/components/solar_logger/include
    ${CMAKE_SOURCE_DIR}/components/cJSON/include
    ${CMAKE_SOURCE_DIR}/components/fronius_solar_api/include
)
