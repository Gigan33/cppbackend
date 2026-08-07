cmake_minimum_required(VERSION 3.11)
project(book_manager CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
find_package(libpqxx REQUIRED)

add_executable(book_manager main.cpp)

target_link_libraries(book_manager PRIVATE
    Boost::headers
    libpqxx::pqxx
)