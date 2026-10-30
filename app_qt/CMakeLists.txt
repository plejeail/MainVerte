find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

include("${PROJECT_SOURCE_DIR}/cmake/translate.cmake")

qt_standard_project_setup(REQUIRES 6.12
    I18N_SOURCE_LANGUAGE en
    I18N_TRANSLATED_LANGUAGES fr
)

file(GLOB_RECURSE APP_CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE APP_QML_FILES CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
)

if(ANDROID)
    list(FILTER APP_CPP_FILES EXCLUDE REGEX
        "desktop[.](cpp|h)$")
    list(FILTER APP_QML_FILES EXCLUDE REGEX
        "desktop[.]qml$")
else()
    list(FILTER APP_CPP_FILES EXCLUDE REGEX
        "android[.](cpp|h)$")
    list(FILTER APP_QML_FILES EXCLUDE REGEX
        "android[.]qml$")
endif()

list(SORT APP_QML_FILES)

foreach(qml_file IN LISTS APP_QML_FILES)
    file(RELATIVE_PATH qml_alias "${CMAKE_CURRENT_SOURCE_DIR}/qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/${qml_file}")
    set_source_files_properties("${qml_file}" PROPERTIES
        QT_RESOURCE_ALIAS "${qml_alias}"
    )
endforeach()

qt_add_executable(appMainVerte
    ${APP_CPP_FILES}
)

qt_add_resources(appMainVerte "img"
    PREFIX "/"
    FILES
        img/flag_en.svg
        img/flag_fr.svg
)

target_include_directories(appMainVerte PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

qt_add_qml_module(appMainVerte
    URI MainVerte
    QML_FILES ${APP_QML_FILES}
)

generate_translations(
    TARGET appMainVerte
    DESTINATION generated_translations.hpp
    DICTIONARIES
        translations/MainVerte_en.dict
        translations/MainVerte_fr.dict
)

set_target_properties(appMainVerte PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMainVerte
    PRIVATE Qt6::Quick
    Qt6::QuickControls2

    mainverte_core
    project_warnings
)

install(TARGETS appMainVerte
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_TESTING)
    qt_add_executable(mainverte_setup_tests
        "${CMAKE_CURRENT_BINARY_DIR}/generated/generated_translations.hpp"
        tests/test_setup_language.cpp
        src/app_settings.cpp src/app_settings.h
        src/widget_wizard_setup.cpp src/widget_wizard_setup.h
        src/translator.cpp src/translator.h src/platform.cpp
    )
    qt_add_resources(mainverte_setup_tests "test_flags" PREFIX "/" FILES img/flag_en.svg img/flag_fr.svg)
    target_include_directories(mainverte_setup_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    add_dependencies(mainverte_setup_tests appMainVerte)
    target_link_libraries(mainverte_setup_tests PRIVATE Qt6::Quick mainverte_core doctest::doctest project_warnings)
    foreach(locale IN ITEMS fr_FR en_GB)
        add_test(NAME setup_language_${locale} COMMAND mainverte_setup_tests)
        set_tests_properties(setup_language_${locale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LANG=${locale}.UTF-8;LC_ALL=${locale}.UTF-8;LANGUAGE=${locale}")
    endforeach()
endif()
