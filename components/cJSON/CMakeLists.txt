add_library(cJSON
    cJSON.c
)

target_include_directories(cJSON PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

#target_link_libraries(cJSON PUBLIC
#)
