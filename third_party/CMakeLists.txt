set(SQLITE_VERSION 3540100)

# doctest is header-only. Add its CMake target for project tests, but don't
# build doctest's own test suite or optional static main library.
set(DOCTEST_WITH_TESTS OFF)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB ON)
set(DOCTEST_NO_INSTALL ON)
add_subdirectory(doctest EXCLUDE_FROM_ALL)

# Build SQLite from its official amalgamation so it is compiled for the
# current toolchain and target platform (desktop, Android, or iOS).
add_library(sqlite3_vendor STATIC
    "sqlite-amalgamation-${SQLITE_VERSION}/sqlite3.c"
)

target_compile_definitions(sqlite3_vendor    PRIVATE
    SQLITE_THREADSAFE=0
    SQLITE_DQS=0
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_AUTOINIT
    SQLITE_DEFAULT_MEMSTATUS=0
    # SQLITE_MAX_EXPR_DEPTH=0
    # SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    # SQLITE_OMIT_COMPILEOPTION_DIAGS
    # SQLITE_OMIT_PROGRESS_CALLBACK
)

target_include_directories(sqlite3_vendor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/sqlite-amalgamation-${SQLITE_VERSION}"
)

set_target_properties(sqlite3_vendor PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
