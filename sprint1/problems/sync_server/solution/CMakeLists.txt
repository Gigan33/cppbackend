cmake_minimum_required(VERSION 3.10)
project(sync_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Находим Boost
find_package(Boost REQUIRED COMPONENTS system)

# Добавляем все cpp файлы из src
file(GLOB SOURCES "src/*.cpp")

add_executable(sync_server ${SOURCES})

target_link_libraries(sync_server Boost::system pthread)
