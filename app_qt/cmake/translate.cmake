
function(update_translation)
    set(oneValueArgs TARGET TS_DIR)
    set(multiValueArgs TRANSLATABLE_QML)

    cmake_parse_arguments(
        PARSE_ARGV 0 arg
        ""
        "${oneValueArgs}"
        "${multiValueArgs}"
    )

    if(NOT arg_TARGET OR NOT arg_TS_DIR)
        message(FATAL_ERROR
            "update_translation requires TARGET and TS_DIR"
        )
    endif()

    if(NOT TARGET "${arg_TARGET}")
        message(FATAL_ERROR
            "Target '${arg_TARGET}' does not exist"
        )
    endif()

    if(NOT arg_TRANSLATABLE_QML)
        message(FATAL_ERROR
            "No translatable QML files provided"
        )
    endif()

    # Find existing translation files
    file(GLOB TS_FILES CONFIGURE_DEPENDS
        "${arg_TS_DIR}/*.ts"
    )

    if(NOT TS_FILES)
        message(FATAL_ERROR
            "No .ts files found in ${arg_TS_DIR}"
        )
    endif()

    # Convert QML paths to absolute paths
    set(QML_FILES)
    foreach(qml IN LISTS arg_TRANSLATABLE_QML)
        get_filename_component(qml_abs "${qml}"
            ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
        )

        list(APPEND QML_FILES "${qml_abs}")
    endforeach()

    # Unique stamp for this target
    set(UPDATE_STAMP
        "${CMAKE_CURRENT_BINARY_DIR}/${arg_TARGET}_translations.stamp"
    )

    # QML changed -> update all TS files
    add_custom_command(
        OUTPUT "${UPDATE_STAMP}"

        COMMAND Qt6::lupdate
        ${QML_FILES}
        -locations none
        -ts ${TS_FILES}

        COMMAND "${CMAKE_COMMAND}" -E touch
        "${UPDATE_STAMP}"

        DEPENDS ${QML_FILES}

        COMMENT "Updating translations for ${arg_TARGET}"
        VERBATIM
    )

    set(QM_FILES)

    # TS changed -> compile corresponding QM
    foreach(ts IN LISTS TS_FILES)

        get_filename_component(name "${ts}" NAME_WE)

        set(qm
            "${CMAKE_CURRENT_BINARY_DIR}/${name}.qm"
        )

        add_custom_command(
            OUTPUT "${qm}"

            COMMAND Qt6::lrelease
            "${ts}"
            -qm "${qm}"

            DEPENDS
                "${ts}"
                "${UPDATE_STAMP}"

            COMMENT "Compiling ${name}.qm"
            VERBATIM
        )

        list(APPEND QM_FILES "${qm}")

    endforeach()

    add_custom_target("${arg_TARGET}_compile_translations"
        DEPENDS ${QM_FILES}
    )

    add_dependencies(
        "${arg_TARGET}"
        "${arg_TARGET}_compile_translations"
    )

    # Embed translations in Qt resources
    qt_add_resources("${arg_TARGET}"
        "${arg_TARGET}_translations"
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${QM_FILES}
    )

endfunction()
