set(SQLITE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)

add_library(sqlite
    ${SQLITE_ROOT}/sqlite3.c)

target_include_directories(sqlite
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${SQLITE_ROOT}>)
