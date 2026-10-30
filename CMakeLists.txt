cmake_minimum_required(VERSION 3.15)
project(nexus_matching_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(nexus_matching_core
    src/main.cpp
    src/OrderBook.cpp
)

target_compile_options(nexus_matching_core PRIVATE -O3)
