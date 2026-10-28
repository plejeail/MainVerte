function(generate_translations)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "TARGET" "DICTIONARIES")
    if(NOT arg_TARGET OR NOT TARGET "${arg_TARGET}" OR NOT arg_DICTIONARIES
       OR arg_UNPARSED_ARGUMENTS OR arg_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "generate_translations requires TARGET and ordered DICTIONARIES")
    endif()

    # The interpreter runs on the build host, also when targeting Android.
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    set(generator "${PROJECT_SOURCE_DIR}/tools/generate_translations.py")
    set(dictionaries)
    foreach(dictionary IN LISTS arg_DICTIONARIES)
        get_filename_component(dictionary "${dictionary}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND dictionaries "${dictionary}")
    endforeach()

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(headers "${output_dir}/generated_translations.hpp")
    set(stamp "${output_dir}/translations.stamp")
    add_custom_command(
        OUTPUT "${stamp}"
        BYPRODUCTS ${headers}
        COMMAND "${Python3_EXECUTABLE}" "${generator}"
            --output-dir "${output_dir}" ${dictionaries}
        COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
        DEPENDS "${generator}" ${dictionaries}
        COMMENT "Generating embedded translations for ${arg_TARGET}"
        VERBATIM
    )
    add_custom_target("${arg_TARGET}_generate_translations" DEPENDS "${stamp}")
    add_dependencies("${arg_TARGET}" "${arg_TARGET}_generate_translations")
    target_sources("${arg_TARGET}" PRIVATE ${headers})
    target_include_directories("${arg_TARGET}" PRIVATE "${output_dir}")

    if(BUILD_TESTING)
        add_test(NAME translation_generator
            COMMAND "${Python3_EXECUTABLE}" -B
                "${PROJECT_SOURCE_DIR}/tools/tests/test_generate_translations.py")
    endif()
endfunction()
