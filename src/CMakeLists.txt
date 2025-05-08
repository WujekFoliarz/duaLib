# CMakeList.txt : CMake project for duaLib, include source and define
# project specific logic here.
#

cmake_minimum_required(VERSION 3.10...3.31)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_executable ("${CMAKE_PROJECT_NAME}")
target_sources("${CMAKE_PROJECT_NAME}" PRIVATE ${MY_SOURCES} )

target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_include_directories("${CMAKE_PROJECT_NAME}" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source/")

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME}
  PRIVATE
    Threads::Threads
    hidapi::hidapi
)

add_subdirectory(thirdparty/hidapi)
target_link_libraries("${CMAKE_PROJECT_NAME}" PRIVATE hidapi::hidapi)
target_compile_features(duaLib PUBLIC cxx_std_20)
