set(LUA54_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.4)

add_library(lua54 STATIC
    ${LUA54_ROOT}/src/lapi.c
    ${LUA54_ROOT}/src/lcode.c
    ${LUA54_ROOT}/src/lctype.c
    ${LUA54_ROOT}/src/ldebug.c
    ${LUA54_ROOT}/src/ldo.c
    ${LUA54_ROOT}/src/ldump.c
    ${LUA54_ROOT}/src/lfunc.c
    ${LUA54_ROOT}/src/lgc.c
    ${LUA54_ROOT}/src/llex.c
    ${LUA54_ROOT}/src/lmem.c
    ${LUA54_ROOT}/src/lobject.c
    ${LUA54_ROOT}/src/lopcodes.c
    ${LUA54_ROOT}/src/lparser.c
    ${LUA54_ROOT}/src/lstate.c
    ${LUA54_ROOT}/src/lstring.c
    ${LUA54_ROOT}/src/ltable.c
    ${LUA54_ROOT}/src/ltm.c
    ${LUA54_ROOT}/src/lundump.c
    ${LUA54_ROOT}/src/lvm.c
    ${LUA54_ROOT}/src/lzio.c
    ${LUA54_ROOT}/src/lauxlib.c
    ${LUA54_ROOT}/src/lbaselib.c
    ${LUA54_ROOT}/src/lcorolib.c
    ${LUA54_ROOT}/src/ldblib.c
    ${LUA54_ROOT}/src/liolib.c
    ${LUA54_ROOT}/src/lmathlib.c
    ${LUA54_ROOT}/src/loadlib.c
    ${LUA54_ROOT}/src/loslib.c
    ${LUA54_ROOT}/src/lstrlib.c
    ${LUA54_ROOT}/src/ltablib.c
    ${LUA54_ROOT}/src/lutf8lib.c
    ${LUA54_ROOT}/src/linit.c)

target_include_directories(lua54
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${LUA54_ROOT}/src>)

if (UNIX)
    target_compile_options(lua54 PRIVATE -DLUA_USE_LINUX)
    target_link_libraries(lua54 PRIVATE m)
endif ()
