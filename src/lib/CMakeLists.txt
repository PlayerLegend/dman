add_library(display_manager_lib)

# Dependencies

find_package(PkgConfig REQUIRED)

pkg_check_modules(X11 REQUIRED x11)
target_include_directories(display_manager_lib PUBLIC ${X11_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${X11_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${X11_LIBRARIES})

pkg_check_modules(XRANDR REQUIRED xrandr)
target_include_directories(display_manager_lib PUBLIC ${XRANDR_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${XRANDR_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${XRANDR_LIBRARIES})

pkg_check_modules(GCRYPT REQUIRED libgcrypt)
target_include_directories(display_manager_lib PUBLIC ${GCRYPT_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${GCRYPT_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${GCRYPT_LIBRARIES})

pkg_check_modules(XI REQUIRED xi)
target_include_directories(display_manager_lib PUBLIC ${XI_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${XI_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${XI_LIBRARIES})

pkg_check_modules(EVDEV REQUIRED libevdev)
target_include_directories(display_manager_lib PUBLIC ${EVDEV_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${EVDEV_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${EVDEV_LIBRARIES})

pkg_check_modules(WLROOTS REQUIRED wlroots-0.20)
target_include_directories(display_manager_lib PUBLIC ${WLROOTS_INCLUDE_DIRS})
target_link_directories(display_manager_lib PRIVATE ${WLROOTS_LIBRARY_DIRS})
target_link_libraries(display_manager_lib PRIVATE ${WLROOTS_LIBRARIES})

# Sources

target_include_directories(display_manager_lib PUBLIC include)
target_sources(
    display_manager_lib PRIVATE
    src/digest.cpp
    src/display-wlroots.cpp
    src/config.cpp
    src/x11.cpp
    src/evdev.cpp
)

# Tests

enable_testing()
add_subdirectory(test/evdev)
