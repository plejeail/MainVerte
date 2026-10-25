# OUTPUT_HEADER  the generated header path
# GLOB_SQL_FILES the GLOB pattern used to get the list of sql files
# PATTERN        the MATCH pattern used to check valid file names
function(generate_sql_resource)
    set(oneValueArgs OUTPUT_HEADER GLOB_SQL_FILES PATTERN)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "${oneValueArgs}" "")

    file(GLOB SQL_FILES CONFIGURE_DEPENDS "${arg_GLOB_SQL_FILES}")
    list(SORT SQL_FILES)
    set(SQL_ENTRIES "")
    set(SQL_COUNT 0)

    foreach(SQL_FILE IN LISTS SQL_FILES)
        get_filename_component(SQL_NAME "${SQL_FILE}" NAME)

        if(NOT SQL_NAME MATCHES "${arg_PATTERN}")
            # message(FATAL_ERROR "Invalid sql filename: ${SQL_NAME}")
            continue()
        endif()

        set(SQL_VERSION "${CMAKE_MATCH_1}")

        math(EXPR SQL_VERSION_NUMBER "${SQL_VERSION}")
        if(NOT SQL_VERSION_NUMBER EQUAL SQL_COUNT)
            message(FATAL_ERROR
                "Invalid sql order: expected ${SQL_COUNT}, got ${SQL_NAME}"
            )
        endif()

        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${SQL_FILE}"
        )

        file(READ "${SQL_FILE}" SQL_CONTENT)

        string(FIND "${SQL_CONTENT}" ")MV_SQL\"" DELIMITER_POS)
        if(NOT DELIMITER_POS EQUAL -1)
            message(FATAL_ERROR "Invalid SQL delimiter in ${SQL_FILE}")
        endif()

        string(APPEND SQL_ENTRIES
            "    R\"MV_SQL(${SQL_CONTENT})MV_SQL\",\n"
        )

        math(EXPR SQL_COUNT "${SQL_COUNT} + 1")
    endforeach()


    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sql_template.hpp.in"
        "${arg_OUTPUT_HEADER}"
        @ONLY
    )
endfunction()



