find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

include("${PROJECT_SOURCE_DIR}/tools/translate.cmake")

qt_standard_project_setup(REQUIRES 6.12)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml")

add_subdirectory(style)
add_subdirectory(showcase)

option(MV_LOG_ENABLE_CONSOLE "Enable console/Logcat output" ON)
option(MV_LOG_ENABLE_DBG "Enable debug logs" ON)
option(MV_LOG_ENABLE_INF "Enable info logs" ON)
option(MV_LOG_ENABLE_WRN "Enable warning logs" ON)
option(MV_LOG_ENABLE_ERR "Enable error logs" ON)
option(MV_LOG_QT_TYPES_FORMAT "Enable Qt type formatters" ON)
option(MV_LOG_QT_DISABLE_QDEBUG "Undefine Qt logging macros to find remaining calls" OFF)
option(MV_LOG_DISABLED "Disable all logging" OFF)

file(GLOB_RECURSE APP_CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
list(FILTER APP_CPP_FILES EXCLUDE REGEX "/mvlog/.*[.]cpp$")

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

qt_add_executable(mainverte_app_qt
    ${APP_CPP_FILES}
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(mainverte_app_qt PROPERTIES
        QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode"
    )
endif()

target_precompile_headers(mainverte_app_qt PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
)

if(WIN32)
    configure_file(img/app_icon.rc.in app_icon.rc @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc" PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/img/logo_mainverte.ico"
    )
    add_library(mainverte_app_icon OBJECT "${CMAKE_CURRENT_BINARY_DIR}/app_icon.rc")
    target_sources(mainverte_app_qt PRIVATE $<TARGET_OBJECTS:mainverte_app_icon>)
elseif(ANDROID)
    set_target_properties(mainverte_app_qt PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.qtproject.example.appMainVerte"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
    )
endif()

qt_add_resources(mainverte_app_qt "img"
    PREFIX "/"
    FILES
        img/care/watering.svg
        img/care/repotting.svg
        img/care/fertilizing.svg
        img/care/pruning.svg
        img/care/treatment.svg
        img/care/light.svg
        img/care/humidity.svg
        img/care/temperature.svg
        img/logo_mainverte.svg
        img/logo_mainverte.png
        img/flag_en.svg
        img/flag_fr.svg
        img/flag_de.svg
        img/flag_es.svg
)

target_include_directories(mainverte_app_qt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

qt_add_qml_module(mainverte_app_qt
    URI MainVerte
    QML_FILES ${APP_QML_FILES}
)

generate_translations(
    TARGET mainverte_app_qt
    DESTINATION generated_translations.hpp
    DICTIONARIES
        translations/en.dict
        translations/fr.dict
        translations/de.dict
        translations/es.dict
)

set_target_properties(mainverte_app_qt PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(mainverte_app_qt
    PRIVATE Qt6::Quick
    Qt6::QuickControls2
    mainverte_styleplugin
    mainverte_showcaseplugin

    mainverte_core
    project_warnings
)

foreach(setting IN ITEMS MV_LOG_ENABLE_CONSOLE MV_LOG_ENABLE_DBG MV_LOG_ENABLE_INF
        MV_LOG_ENABLE_WRN MV_LOG_ENABLE_ERR MV_LOG_QT_TYPES_FORMAT
        MV_LOG_QT_DISABLE_QDEBUG MV_LOG_DISABLED)
    target_compile_definitions(mainverte_app_qt PUBLIC "${setting}=$<BOOL:${${setting}}>")
endforeach()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS mainverte_app_qt
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
        src/translator.cpp src/translator.h src/language_option.h src/platform.cpp
    )
    qt_add_resources(mainverte_setup_tests "test_flags" PREFIX "/" FILES
        img/flag_en.svg img/flag_fr.svg img/flag_de.svg img/flag_es.svg)
    target_include_directories(mainverte_setup_tests PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generated")
    add_dependencies(mainverte_setup_tests mainverte_app_qt)
    target_link_libraries(mainverte_setup_tests PRIVATE Qt6::Quick mainverte_core mainverte_mvlog sqlite3_vendor doctest::doctest project_warnings)
    # Link the app boundary against a controlled Core completion provider so queue
    # refusal can be verified without changing the production service lifecycle.
    qt_add_executable(mainverte_settings_language_tests
        "${CMAKE_CURRENT_BINARY_DIR}/generated/generated_translations.hpp"
        tests/test_settings_language.cpp
        src/app_settings.cpp src/app_settings.h
        src/translator.cpp src/translator.h src/language_option.h)
    target_include_directories(mainverte_settings_language_tests PRIVATE
        src "${CMAKE_CURRENT_BINARY_DIR}/generated" "${PROJECT_SOURCE_DIR}/core/include")
    add_dependencies(mainverte_settings_language_tests mainverte_app_qt)
    target_link_libraries(mainverte_settings_language_tests PRIVATE
        Qt6::Quick doctest::doctest project_warnings)
    qt_add_resources(mainverte_settings_language_tests "settings_test_flags" PREFIX "/" FILES
        img/flag_en.svg img/flag_fr.svg img/flag_de.svg img/flag_es.svg)
    add_test(NAME settings_language_completion COMMAND mainverte_settings_language_tests)
    set_tests_properties(settings_language_completion PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    foreach(locale IN ITEMS fr_FR en_GB de_DE es_ES)
        add_test(NAME setup_language_${locale} COMMAND mainverte_setup_tests)
        set_tests_properties(setup_language_${locale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LANG=${locale}.UTF-8;LC_ALL=${locale}.UTF-8;LANGUAGE=${locale}")
    endforeach()
endif()
