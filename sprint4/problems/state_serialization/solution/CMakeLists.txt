cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(game_model STATIC
    src/geom.h
    src/model_serialization.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/loot_generator.cpp      
    src/collision_detector.cpp  
)

target_link_libraries(game_model PUBLIC CONAN_PKG::boost Threads::Threads)

# 2. Основной сервер
add_executable(game_server
    src/main.cpp
    src/http_server.cpp 
    src/json_loader.cpp         
    src/logger.cpp             
    src/ticker.cpp
)

target_link_libraries(game_server PRIVATE game_model)

# 3. Тесты
add_executable(game_server_tests
    tests/state-serialization-tests.cpp
    src/http_server.cpp 
    src/json_loader.cpp        
    src/logger.cpp            
    src/ticker.cpp
)

target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 game_model)