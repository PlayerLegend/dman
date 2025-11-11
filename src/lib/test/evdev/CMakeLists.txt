add_executable(evdev.base main.cpp)
target_link_libraries(evdev.base PUBLIC display_manager_lib)
add_test(evdev.base evdev.base)