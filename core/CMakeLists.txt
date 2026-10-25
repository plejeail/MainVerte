include(cmake/cmake_generate_sql.cmake)

file(GLOB_RECURSE CORE_CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(mainverte_core
    ${CORE_CPP_FILES}
)

target_compile_features(mainverte_core PUBLIC
    cxx_std_23
)

generate_sql_resource(
    OUTPUT_HEADER  "${CMAKE_CURRENT_BINARY_DIR}/generated/sql_scripts.hpp"
    GLOB_SQL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/sql/*.sql"
    PATTERN "^([0-9]+)_.+\\.sql$"
)

target_include_directories(mainverte_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(mainverte_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_precompile_headers(mainverte_core PRIVATE
    src/pch.h
)

target_link_libraries(mainverte_core PRIVATE project_warnings)
target_link_libraries(mainverte_core PRIVATE sqlite3_vendor)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
