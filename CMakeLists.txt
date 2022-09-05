cmake_minimum_required(VERSION 3.2)
project(autodo)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(auto_src_list
    src/lua_api.c
    src/lua_screenshot.c
    src/main.c
    src/utils.c)

function(setup_target_wall name)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
    endif ()
endfunction()

add_executable(${PROJECT_NAME}
    ${auto_src_list}
    ${CMAKE_CURRENT_SOURCE_DIR}/share/appicon.rc)

find_package(Lua REQUIRED)

if (WIN32)
else ()
    find_package(X11)
    find_package(Cairo)
    target_include_directories(${PROJECT_NAME}
        PRIVATE
            ${X11_INCLUDE_DIR}
            ${CAIRO_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            ${X11_LIBRARIES}
            ${CAIRO_LIBRARIES})
endif ()


target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        ${LUA_INCLUDE_DIR}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${LUA_LIBRARY})

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")
setup_target_wall(${PROJECT_NAME})

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
endif()
