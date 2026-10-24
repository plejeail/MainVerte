find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.12)

file(GLOB_RECURSE APP_CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE APP_QML_FILES CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
)

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

target_include_directories(appMainVerte PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

qt_add_qml_module(appMainVerte
    URI MainVerte
    QML_FILES ${APP_QML_FILES}
)

set_target_properties(appMainVerte PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appMainVerte
    PRIVATE Qt6::Quick
    mainverte_core
    project_warnings
)

install(TARGETS appMainVerte
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
