cmake_minimum_required(VERSION 3.10)
project(game_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Указываем путь к новой Boost (1.83.0)
set(BOOST_ROOT /usr/local)
set(BOOST_INCLUDEDIR /usr/local/include)
set(BOOST_LIBRARYDIR /usr/local/lib)

find_package(Boost 1.83.0 REQUIRED COMPONENTS system)

# Скачиваем nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(game_server 
    main.cpp 
    json_loader.cpp
)

target_link_libraries(game_server 
    ${Boost_LIBRARIES} 
    pthread
)

target_include_directories(game_server PRIVATE 
    ${Boost_INCLUDE_DIRS}
    ${json_SOURCE_DIR}/include
)
