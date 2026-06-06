cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.78.0 REQUIRED COMPONENTS system)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server 
    src/main.cpp 
    src/json_loader.cpp 
    src/request_handler.cpp 
    src/model.cpp
)

target_link_libraries(game_server 
    ${Boost_LIBRARIES}
    Threads::Threads
)

target_include_directories(game_server PRIVATE ${Boost_INCLUDE_DIRS})

# Заглушка для C компилятора (не используется)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_C_COMPILER gcc)
