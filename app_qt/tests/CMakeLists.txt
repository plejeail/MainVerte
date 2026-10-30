find_package(Qt6 REQUIRED COMPONENTS QuickTest)
qt_add_executable(mainverte_style_tests style_test_main.cpp)
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
    Qt6::QuickTest Qt6::QuickControls2 mainverte_styleplugin mainverte_showcaseplugin project_warnings)
target_compile_definitions(mainverte_style_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
add_test(NAME mainverte_style COMMAND mainverte_style_tests)
set_tests_properties(mainverte_style PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
