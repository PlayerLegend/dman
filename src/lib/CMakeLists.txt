add_library(display_manager_lib)

find_package(PkgConfig REQUIRED)

# X11

find_package(X11 REQUIRED)
target_include_directories(display_manager_lib PUBLIC ${X11_INCLUDE_DIR})
target_link_libraries(display_manager_lib PUBLIC ${X11_LIBRARIES})

# Xrandr

find_library(XRANDR_LIB NAMES Xrandr)

if(NOT XRANDR_LIB)
    message(FATAL_ERROR "libXrandr (Xrandr) not found on this system")
endif()

target_link_libraries(display_manager_lib PUBLIC ${XRANDR_LIB})

# libgcrypt

find_library(GCRYPT_LIB NAMES gcrypt)
find_path(GCRYPT_INCLUDE NAMES gcrypt.h)
if(NOT GCRYPT_LIB)
    message(FATAL_ERROR "libgcrypt (gcrypt) not found on this system")
endif()

target_link_libraries(display_manager_lib PUBLIC ${GCRYPT_LIB})

if(NOT GCRYPT_INCLUDE)
    message(FATAL_ERROR "libgcrypt include files not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${GCRYPT_INCLUDE})

# XInput

find_library(XI_LIB NAMES Xi Xinput)
find_path(XI_INCLUDE NAMES X11/extensions/XInput2.h)
if(NOT XI_LIB)
    message(FATAL_ERROR "libXi (X Input extension) not found on this system")
endif()

target_link_libraries(display_manager_lib PUBLIC ${XI_LIB})

if(NOT XI_INCLUDE)
    message(FATAL_ERROR "XInput include files not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${XI_INCLUDE})

# libevdev
pkg_check_modules(EVDEV REQUIRED libevdev)
separate_arguments(EVDEV_INCLUDE_DIRS NATIVE_COMMAND "${EVDEV_INCLUDE_DIRS}")

if(NOT EVDEV_LIBRARIES)
    message(FATAL_ERROR "libevdev (evdev) not found on this system")
endif()

target_link_libraries(display_manager_lib PUBLIC ${EVDEV_LIBRARIES})

if(NOT EVDEV_INCLUDE_DIRS)
    message(FATAL_ERROR "libevdev include files not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${EVDEV_INCLUDE_DIRS})

# Sources

target_include_directories(display_manager_lib PUBLIC include)
target_sources(
    display_manager_lib PRIVATE
    src/digest.cpp
    src/display.cpp
    src/config.cpp
    src/x11.cpp
    src/evdev.cpp
)

# Tests

enable_testing()
add_subdirectory(test/evdev)