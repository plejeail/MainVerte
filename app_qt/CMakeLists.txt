find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

include("${PROJECT_SOURCE_DIR}/cmake/translate.cmake")

qt_standard_project_setup(REQUIRES 6.12)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml")

add_subdirectory(style)
add_subdirectory(showcase)

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

if(WIN32)
    configure_file(img/app_icon.rc.in app_icon.rc @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/img/logo_mainverte.ico"
    )
    add_library(mainverte_app_icon OBJECT "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    target_sources(appMainVerte PRIVATE $<TARGET_OBJECTS:mainverte_app_icon>)
elseif(ANDROID)
    set_target_properties(appMainVerte PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
    )
endif()

qt_add_resources(appMainVerte "img"
    PREFIX "/"
    FILES
        img/logo_mainverte.svg
        img/logo_mainverte.png
        img/flag_en.svg
        img/flag_fr.svg
        img/flag_de.svg
        img/flag_es.svg
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
        translations/en.dict
        translations/fr.dict
        translations/de.dict
        translations/es.dict
)

set_target_properties(appMainVerte PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMainVerte
    PRIVATE Qt6::Quick
    Qt6::QuickControls2
    mainverte_styleplugin
    mainverte_showcaseplugin

    mainverte_core
    project_warnings
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

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
    qt_add_resources(mainverte_setup_tests "test_flags" PREFIX "/" FILES
        img/flag_en.svg img/flag_fr.svg img/flag_de.svg img/flag_es.svg)
    target_include_directories(mainverte_setup_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    add_dependencies(mainverte_setup_tests appMainVerte)
    target_link_libraries(mainverte_setup_tests PRIVATE Qt6::Quick mainverte_core doctest::doctest project_warnings)
    foreach(locale IN ITEMS fr_FR en_GB de_DE es_ES)
        add_test(NAME setup_language_${locale} COMMAND mainverte_setup_tests)
        set_tests_properties(setup_language_${locale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LANG=${locale}.UTF-8;LC_ALL=${locale}.UTF-8;LANGUAGE=${locale}")
    endforeach()
endif()
