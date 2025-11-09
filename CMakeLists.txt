cmake_minimum_required(VERSION 3.12)
project(display_manager_lib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(display_manager_lib)

# target_include_directories(display_manager_lib PUBLIC include)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

target_compile_features(display_manager_lib PUBLIC cxx_std_17)

target_sources(display_manager_lib PRIVATE
    src/digest.cpp
    src/dman.cpp
)

add_executable(dman src/util.cpp)

target_link_libraries(dman PRIVATE display_manager_lib)

# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION include)
