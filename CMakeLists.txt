cmake_minimum_required(VERSION 3.16)

project(MainVerte VERSION 0.1 LANGUAGES C CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(project_warnings INTERFACE)

if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(third_party)
add_subdirectory(core)
add_subdirectory(app_qt)
