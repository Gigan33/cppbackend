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

add_library(game_model STATIC
    src/model.cpp
    src/json_loader.cpp
    src/loot_generator.cpp
    src/collision_detector.cpp
)

target_include_directories(game_model PUBLIC 
    src
    CONAN_PKG::boost
)

target_link_libraries(game_model PUBLIC 
    CONAN_PKG::boost
)

add_executable(game_server
    src/main.cpp
    src/logger.cpp
    src/ticker.cpp
    src/http_server.cpp
)

target_link_libraries(game_server 
    PRIVATE 
    game_model
    Threads::Threads
    CONAN_PKG::boost
    pthread
    dl
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    include(CTest)

    if(DEFINED CONAN_BUILD_DIRS_CATCH2_DEBUG)
        include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
    elseif(DEFINED CONAN_BUILD_DIRS_CATCH2)
        include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
    endif()

    add_executable(game_server_tests
        tests/model_tests.cpp # Укажите ваш файл с тестами
    )

    target_link_libraries(game_server_tests
        PRIVATE
        game_model
        CONAN_PKG::catch2
        Threads::Threads
        pthread
        dl
    )

    catch_discover_tests(game_server_tests)
endif()