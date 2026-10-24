file(GLOB_RECURSE CORE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(mainverte_core_tests
    ${CORE_TEST_SOURCES}
)

target_link_libraries(mainverte_core_tests
    PRIVATE
        mainverte_core
        doctest::doctest_with_main
        project_warnings
)

add_test(NAME mainverte_core_tests COMMAND mainverte_core_tests)
