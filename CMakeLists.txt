cmake_minimum_required(VERSION 3.1)
project(snake C)

set(CMAKE_C_STANDARD 11)

# Устанавливаем тип сборки по умолчанию на Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

add_executable(snake main.c)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

    add_custom_command(TARGET snake POST_BUILD
            COMMAND ${CMAKE_STRIP} $<TARGET_FILE:snake>
    )
endif()

install(TARGETS snake DESTINATION bin)