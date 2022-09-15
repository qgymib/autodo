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

function(setup_required_package_lua name)
    find_package(Lua REQUIRED)
    target_include_directories(${name}
        PRIVATE
            ${LUA_INCLUDE_DIR})
    target_link_libraries(${name}
        PRIVATE
            ${LUA_LIBRARY})
endfunction()

function(setup_required_package_x11 name)
    find_package(X11 REQUIRED)
    target_include_directories(${name}
        PRIVATE
            ${X11_INCLUDE_DIR})
    target_link_libraries(${name}
        PRIVATE
            ${X11_LIBRARIES})
endfunction()

function(setup_required_package_cairo name)
    find_package(Cairo REQUIRED)
    target_include_directories(${name}
        PRIVATE
            ${CAIRO_INCLUDE_DIRS})
    target_link_libraries(${name}
        PRIVATE
            ${CAIRO_LIBRARIES})
endfunction()

###############################################################################
# Setup executable
###############################################################################

set(auto_src_list
    src/lua/api.c
    src/lua/coroutine.c
    src/lua/process.c
    src/lua/screenshot.c
    src/lua/sleep.c
    src/utils/list.c
    src/utils/map.c
    src/main.c
    src/runtime.c
    src/utils.c)

add_executable(${PROJECT_NAME}
    ${auto_src_list}
    ${CMAKE_CURRENT_SOURCE_DIR}/share/appicon.rc)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")
setup_target_wall(${PROJECT_NAME})

###############################################################################
# Dependency
###############################################################################

# Lua
setup_required_package_lua(${PROJECT_NAME})

# libuv
add_subdirectory(third_party/libuv)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        uv_a)

if (WIN32)
else ()
    setup_required_package_x11(${PROJECT_NAME})
    setup_required_package_cairo(${PROJECT_NAME})
endif ()

###############################################################################
# Test
###############################################################################

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
endif()
