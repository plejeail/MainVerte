file(GLOB_RECURSE CORE_CPP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(mainverte_core
    ${CORE_CPP_FILES}
)

target_include_directories(mainverte_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(mainverte_core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(mainverte_core PUBLIC
    cxx_std_23
)

target_link_libraries(mainverte_core PRIVATE project_warnings)
target_link_libraries(mainverte_core PRIVATE sqlite3_vendor)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
