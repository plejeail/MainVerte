find_package(Qt6 REQUIRED COMPONENTS QuickTest)
if(MV_LOG_QT_TYPES_FORMAT)
    add_executable(mainverte_log_qt_tests test_log_qt_formatters.cpp)
    target_link_libraries(mainverte_log_qt_tests PRIVATE mainverte_mvlog doctest::doctest_with_main project_warnings)
    add_test(NAME mainverte_log_qt COMMAND mainverte_log_qt_tests)
    if(MINGW)
        get_filename_component(log_qt_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_target_property(log_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        get_filename_component(log_qt_bin_directory "${log_qt_core_location}" DIRECTORY)
        set_tests_properties(mainverte_log_qt PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${log_qt_bin_directory};PATH=path_list_prepend:${log_qt_runtime_directory}")
    endif()
endif()

add_executable(mainverte_log_date_tests test_log_date.cpp)
target_link_libraries(mainverte_log_date_tests PRIVATE doctest::doctest_with_main project_warnings)
add_test(NAME mainverte_log_date COMMAND mainverte_log_date_tests)
if(MINGW)
    get_filename_component(log_test_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(mainverte_log_date PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${log_test_runtime_directory}")
endif()

add_executable(mainverte_log_file_tests test_log_file_stream.cpp)
target_link_libraries(mainverte_log_file_tests PRIVATE doctest::doctest_with_main project_warnings)
add_test(NAME mainverte_log_file COMMAND mainverte_log_file_tests)
if(MINGW)
    set_tests_properties(mainverte_log_file PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${log_test_runtime_directory}")
endif()

if(NOT ANDROID)
    add_executable(mainverte_log_android_output_tests
        test_log_android_output.cpp ../src/mvlog/mvlog.android.cpp)
    target_include_directories(mainverte_log_android_output_tests PRIVATE support)
    target_compile_definitions(mainverte_log_android_output_tests PRIVATE MV_LOG_QT_TYPES_FORMAT=0)
    target_link_libraries(mainverte_log_android_output_tests PRIVATE
        doctest::doctest_with_main project_warnings)
    add_test(NAME mainverte_log_android_output COMMAND mainverte_log_android_output_tests)
    add_executable(mainverte_log_desktop_tests test_log_desktop.cpp ../src/mvlog/mvlog.cpp ../src/mvlog/mvlog.desktop.cpp)
    target_link_libraries(mainverte_log_desktop_tests PRIVATE
        doctest::doctest_with_main project_warnings)
    target_compile_definitions(mainverte_log_desktop_tests PRIVATE MV_LOG_QT_TYPES_FORMAT=0)
    add_test(NAME mainverte_log_desktop COMMAND mainverte_log_desktop_tests)
    if(MINGW)
        set_tests_properties(mainverte_log_android_output PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${log_test_runtime_directory}")
        set_tests_properties(mainverte_log_desktop PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${log_test_runtime_directory}")
    endif()
endif()

qt_add_executable(mainverte_style_tests style_test_main.cpp)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(mainverte_style_tests PROPERTIES
        QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode"
    )
endif()
# Exercise the production window and its translations, without starting Core.
set(app_translation_header "${CMAKE_CURRENT_BINARY_DIR}/../generated/generated_translations.hpp")
set_source_files_properties("${app_translation_header}" PROPERTIES GENERATED TRUE)
target_sources(mainverte_style_tests PRIVATE
    ../src/translator.cpp ../src/translator.h "${app_translation_header}")
target_include_directories(mainverte_style_tests PRIVATE
    ../src "${CMAKE_CURRENT_BINARY_DIR}/../generated")
add_dependencies(mainverte_style_tests appMainVerte_generate_translations)
foreach(qml_file IN ITEMS Main.qml CollectionPage.qml SettingsPage.qml)
    set_source_files_properties("../qml/${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_file}")
endforeach()
qt_add_qml_module(mainverte_style_tests
    URI MainVerte
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/MainVerte"
    QML_FILES ../qml/Main.qml ../qml/CollectionPage.qml ../qml/SettingsPage.qml
)
target_link_libraries(mainverte_style_tests PRIVATE
    Qt6::QuickTest Qt6::QuickControls2 mainverte_styleplugin mainverte_showcaseplugin mainverte_mvlog project_warnings)
target_compile_definitions(mainverte_style_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME mainverte_style COMMAND mainverte_style_tests)
set_tests_properties(mainverte_style PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
