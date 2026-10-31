qt_add_library(mainverte_showcase STATIC)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(mainverte_showcase PROPERTIES
        QT_QMLCACHEGEN_ARGUMENTS "--only-bytecode"
    )
endif()
qt_add_qml_module(mainverte_showcase
    URI MainVerte.Showcase
    CLASS_NAME MainVerteShowcasePlugin
    QML_FILES Showcase.qml Section.qml StatusBadge.qml Measurement.qml
)
target_link_libraries(mainverte_showcase PRIVATE Qt6::QuickControls2 mainverte_style)
