# Specify the minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Set the project name
project(modbus_proxy)

# Specify the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the components
add_subdirectory(components/logging)
add_subdirectory(components/modbus_tcp_client)
add_subdirectory(components/modbus_tcp_server)
add_subdirectory(components/modbus_tcp_poll)
add_subdirectory(components/http_server)
add_subdirectory(main)

# Define the executable and link all components
add_executable(modbus_proxy
    main/main.c
)

# Link libraries to the executable
target_link_libraries(modbus_proxy
    modbus_tcp_client
    modbus_tcp_server
    logging
    http_server
)

# Include directories for the main target
target_include_directories(modbus_proxy PUBLIC
    ${CMAKE_SOURCE_DIR}/main/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_client/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_server/include
    ${CMAKE_SOURCE_DIR}/components/modbus_tcp_poll/include
    ${CMAKE_SOURCE_DIR}/components/logging/include
    ${CMAKE_SOURCE_DIR}/components/http_server/include
)
