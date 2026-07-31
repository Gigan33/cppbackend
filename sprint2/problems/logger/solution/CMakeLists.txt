cmake_minimum_required(VERSION 3.9)
project(HelloLog CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

add_executable(hello_log src/main.cpp)

target_include_directories(hello_log PRIVATE src)
target_link_libraries(hello_log CONAN_PKG::boost)
