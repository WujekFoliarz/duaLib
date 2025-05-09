﻿cmake_minimum_required(VERSION 3.10...3.31)
project(duaLib LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_executable(${PROJECT_NAME} ${MY_SOURCES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
)

add_subdirectory(thirdparty/hidapi)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Threads::Threads
    hidapi::hidapi
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:hidapi::hidapi> $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
