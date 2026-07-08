cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/json_loader.cpp
    src/model.cpp
    src/logger.cpp
    src/ticker.cpp
    src/http_server.cpp
)

target_include_directories(game_server PRIVATE CONAN_PKG::boost)

target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads
    CONAN_PKG::boost
    pthread
    dl
)