cmake_minimum_required(VERSION 3.12)
project(display_manager_lib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(display_manager_lib)

# Find X11 (headers & libs) and libXrandr, then expose them on the library target
find_package(X11 REQUIRED)
find_library(XRANDR_LIB NAMES Xrandr)
find_library(GCRYPT_LIB NAMES gcrypt)
find_path(GCRYPT_INCLUDE NAMES gcrypt.h)

if(NOT XRANDR_LIB)
    message(FATAL_ERROR "libXrandr (Xrandr) not found on this system")
endif()

if(NOT GCRYPT_LIB)
    message(FATAL_ERROR "libgcrypt (gcrypt) not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${X11_INCLUDE_DIR})
if(NOT GCRYPT_INCLUDE)
    message(FATAL_ERROR "libgcrypt include files not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${GCRYPT_INCLUDE})
target_link_libraries(display_manager_lib PUBLIC ${XRANDR_LIB} ${X11_LIBRARIES} ${GCRYPT_LIB})

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
