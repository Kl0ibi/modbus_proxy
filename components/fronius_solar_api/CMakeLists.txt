
add_library(fronius_solar_api
    fronius_solar_api.c
)

target_include_directories(fronius_solar_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fronius_solar_api PUBLIC
    system
    logging
    cJSON
    curl
)
