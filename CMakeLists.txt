cmake_minimum_required(VERSION 3.2)
project(autodo)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

###############################################################################
# Functions
###############################################################################

# Enable all warning for target
function(setup_target_wall name)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    endif ()
endfunction()

# Set default visibility as hidden
function(setup_target_hidden name)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${name} PRIVATE -fvisibility=hidden)
    endif ()
endfunction()

###############################################################################
# Setup executable
###############################################################################

add_executable(${PROJECT_NAME}
    src/api/api.c
    src/api/async.c
    src/api/coroutine.c
    src/api/list.c
    src/api/lua.c
    src/api/map.c
    src/api/memory.c
    src/api/misc.c
    src/api/notify.c
    src/api/regex.c
    src/api/sem.c
    src/api/thread.c
    src/api/timer.c
    src/lua/api.c
    src/lua/coroutine.c
    src/lua/download.c
    src/lua/fs.c
    src/lua/json.c
    src/lua/process.c
    src/lua/regex.c
    src/lua/sleep.c
    src/lua/sqlite.c
    src/lua/uname.c
    src/utils/list.c
    src/utils/map.c
    src/main.c
    src/package.c
    src/runtime.c
    src/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/share/appicon.rc)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Shlwapi)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif ()

setup_target_wall(${PROJECT_NAME})
setup_target_hidden(${PROJECT_NAME})

###############################################################################
# Dependency
###############################################################################

# Lua
include(third_party/lua54.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE lua54)

# libuv
add_subdirectory(third_party/libuv)
target_link_libraries(${PROJECT_NAME} PRIVATE uv_a)

# cJSON
include(third_party/cJSON.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE cJSON)

# PCRE2
include(third_party/pcre2.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE pcre2)

# SQLite
include(third_party/sqlite.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite)

# CSV Parser
include(third_party/csvparser.cmake)
target_link_libraries(${PROJECT_NAME} PRIVATE csvparser)

###############################################################################
# Test
###############################################################################

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
endif()
