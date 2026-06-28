cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

# Включаем C++20, так как мы используем новые фичи (например, std::optional)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

# Настраиваем потоки
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# --- ВМЕСТО МАКРОСОВ CONAN ИСПОЛЬЗУЕМ СТАНДАРТНЫЙ ПОИСК BOOST ---
# Проверяющая система Практикума уже положила библиотеки Boost в систему.
# Нам нужно явно сказать, что мы ищем компонент program_options
find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(game_server
    src/main.cpp
    src/json_loader.cpp
    src/model.cpp
    src/logger.cpp
)

# Подключаем папки с заголовочными файлами Boost
target_include_directories(game_server PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(game_server 
    PRIVATE 
    Threads::Threads
    ${Boost_LIBRARIES} # Линкуем библиотеки Boost (включая program_options)
    pthread
    dl
)