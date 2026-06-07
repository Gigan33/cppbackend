cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Заглушка для C компилятора
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER gcc)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/json_loader.cpp
    src/model.cpp
)

target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads
    ${CONAN_LIBS}
)
