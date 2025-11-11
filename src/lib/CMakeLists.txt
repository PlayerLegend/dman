add_library(display_manager_lib)

find_package(X11 REQUIRED)
find_library(XRANDR_LIB NAMES Xrandr)
find_library(GCRYPT_LIB NAMES gcrypt)
find_path(GCRYPT_INCLUDE NAMES gcrypt.h)

# Add: find X Input (libXi) — commonly the "xinput" extension library
find_library(XI_LIB NAMES Xi Xinput)
find_path(XI_INCLUDE NAMES X11/extensions/XInput2.h)

if(NOT XRANDR_LIB)
    message(FATAL_ERROR "libXrandr (Xrandr) not found on this system")
endif()

if(NOT GCRYPT_LIB)
    message(FATAL_ERROR "libgcrypt (gcrypt) not found on this system")
endif()

# Check XInput presence
if(NOT XI_LIB)
    message(FATAL_ERROR "libXi (X Input extension) not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${X11_INCLUDE_DIR})
if(NOT GCRYPT_INCLUDE)
    message(FATAL_ERROR "libgcrypt include files not found on this system")
endif()

target_include_directories(display_manager_lib PUBLIC ${GCRYPT_INCLUDE})
# Add XInput include dir
if(NOT XI_INCLUDE)
    message(FATAL_ERROR "XInput include files not found on this system")
endif()
target_include_directories(display_manager_lib PUBLIC ${XI_INCLUDE})

target_link_libraries(display_manager_lib PUBLIC ${XRANDR_LIB} ${X11_LIBRARIES} ${GCRYPT_LIB} ${XI_LIB})

target_include_directories(display_manager_lib PUBLIC include)
target_sources(
    display_manager_lib PRIVATE
    src/digest.cpp
    src/display.cpp
    src/config.cpp
)