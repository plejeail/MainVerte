option(MV_LOG_ENABLE_CONSOLE "Enable console/Logcat output" ON)
option(MV_LOG_ENABLE_DBG "Enable debug logs" ON)
option(MV_LOG_ENABLE_INF "Enable info logs" ON)
option(MV_LOG_ENABLE_WRN "Enable warning logs" ON)
option(MV_LOG_ENABLE_ERR "Enable error logs" ON)
option(MV_LOG_QT_TYPES_FORMAT "Enable Qt type formatters" ON)
option(MV_LOG_QT_DISABLE_QDEBUG "Undefine Qt logging macros to find remaining calls" OFF)
option(MV_LOG_DISABLED "Disable all logging" OFF)

add_library(mainverte_mvlog STATIC mvlog.cpp)
if(ANDROID)
    target_sources(mainverte_mvlog PRIVATE mvlog.android.cpp)
    target_link_libraries(mainverte_mvlog PRIVATE log)
else()
    target_sources(mainverte_mvlog PRIVATE mvlog.desktop.cpp)
endif()

set_target_properties(mainverte_mvlog PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mainverte_mvlog PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(mainverte_mvlog PRIVATE project_warnings)

if(MV_LOG_QT_TYPES_FORMAT)
    target_link_libraries(mainverte_mvlog PUBLIC Qt6::Core)
endif()

foreach(setting IN ITEMS MV_LOG_ENABLE_CONSOLE MV_LOG_ENABLE_DBG MV_LOG_ENABLE_INF
        MV_LOG_ENABLE_WRN MV_LOG_ENABLE_ERR MV_LOG_QT_TYPES_FORMAT
        MV_LOG_QT_DISABLE_QDEBUG MV_LOG_DISABLED)
    target_compile_definitions(mainverte_mvlog PUBLIC "${setting}=$<BOOL:${${setting}}>")
endforeach()
