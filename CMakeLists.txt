cmake_minimum_required(VERSION 3.25)

project(MainVerte VERSION 0.1 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_QT_APP "build a qt application using the core app" ON)

add_library(project_warnings INTERFACE)

if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(third_party)
add_subdirectory(core)
if (BUILD_QT_APP)
    add_subdirectory(app_qt)
endif()

include(CheckIPOSupported)
check_ipo_supported(LANGUAGES CXX)
set_property(
    TARGET mainverte_core
    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)
