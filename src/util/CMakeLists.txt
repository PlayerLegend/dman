set(HELP_TEXT_IN "${CMAKE_CURRENT_SOURCE_DIR}/help.txt")
set(HELP_TEXT_OUT "${CMAKE_CURRENT_BINARY_DIR}/help_txt.cpp")

find_program(XXD_EXECUTABLE NAMES xxd)

if(NOT XXD_EXECUTABLE)
	message(FATAL_ERROR "xxd not found. Please install xxd (e.g. vim-common) or ensure it is on PATH.")
endif()

add_custom_command(
    OUTPUT "${HELP_TEXT_OUT}"
    COMMAND "${XXD_EXECUTABLE}" -n help_txt -i "${HELP_TEXT_IN}" "${HELP_TEXT_OUT}"
    DEPENDS "${HELP_TEXT_IN}"
    COMMENT "Generating ${HELP_TEXT_OUT} from ${HELP_TEXT_IN} (xxd -i)"
    VERBATIM
)

add_custom_target(generate_help_txt DEPENDS "${HELP_TEXT_OUT}")

set_source_files_properties("${HELP_TEXT_OUT}" PROPERTIES GENERATED TRUE)

add_executable(dman src/util.cpp "${HELP_TEXT_OUT}")
add_dependencies(dman generate_help_txt)

target_include_directories(dman PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(dman PRIVATE display_manager_lib)
