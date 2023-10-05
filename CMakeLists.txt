cmake_minimum_required(VERSION 3.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)
include_directories(CMAKE_SOURCE_DIR)

project(MYCJSON)
add_executable(cjson ${CMAKE_SOURCE_DIR}/cJSON.c ${CMAKE_SOURCE_DIR}/main.c)