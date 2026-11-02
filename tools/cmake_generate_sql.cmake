# TARGET consumes OUTPUT_HEADER; PATTERN captures the numeric migration version.
function(generate_sql_resource)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "TARGET;OUTPUT_HEADER;GLOB_SQL_FILES;PATTERN" "")
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    set(generator "${PROJECT_SOURCE_DIR}/tools/generate_sql_scripts.py")
    file(GLOB sql_files CONFIGURE_DEPENDS "${arg_GLOB_SQL_FILES}")
    string(JOIN "\n" sql_inputs ${sql_files})
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/sql_inputs.txt"
        CONTENT "${sql_inputs}\n" @ONLY)
    set(stamp "${arg_OUTPUT_HEADER}.stamp")
    add_custom_command(
        OUTPUT "${stamp}"
        BYPRODUCTS "${arg_OUTPUT_HEADER}"
        COMMAND "${Python3_EXECUTABLE}" "${generator}"
            --output "${arg_OUTPUT_HEADER}" --pattern "${arg_PATTERN}" ${sql_files}
        COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
        DEPENDS "${generator}" ${sql_files} "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/sql_inputs.txt"
        COMMENT "Generating embedded SQL for ${arg_TARGET}"
        VERBATIM
    )
    add_custom_target("${arg_TARGET}_generate_sql" DEPENDS "${stamp}")
    add_dependencies("${arg_TARGET}" "${arg_TARGET}_generate_sql")
    target_sources("${arg_TARGET}" PRIVATE "${arg_OUTPUT_HEADER}")
    if(BUILD_TESTING)
        add_test(NAME sql_generator
            COMMAND "${Python3_EXECUTABLE}" -B
                "${PROJECT_SOURCE_DIR}/tools/tests/test_generate_sql_scripts.py")
    endif()
endfunction()
