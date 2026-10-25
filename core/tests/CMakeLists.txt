file(GLOB_RECURSE CORE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(mainverte_core_tests
    ${CORE_TEST_SOURCES}
)

target_include_directories(mainverte_core_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_link_libraries(mainverte_core_tests
    PRIVATE
        mainverte_core
        sqlite3_vendor
        doctest::doctest_with_main
        project_warnings
)

target_precompile_headers(mainverte_core_tests PRIVATE
    "../src/pch.h"
)

add_test(NAME mainverte_core_tests COMMAND mainverte_core_tests)

if(MINGW)
    # Make the compiler's runtime DLLs available when CTest launches the tests.
    get_filename_component(MINGW_RUNTIME_DIRECTORY "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(mainverte_core_tests PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MINGW_RUNTIME_DIRECTORY}"
    )
endif()
