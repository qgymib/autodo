#include <cJSON.h>
#include "lua.h"

static int _lua_A_callk(lua_State* L, int nargs, int nrets, void* ctx, auto_lua_KFunction k)
{
    lua_callk(L, nargs, nrets, (lua_KContext)ctx, (lua_KFunction)(void*)k);
    return k(L, LUA_OK, ctx);
}

static int _lua_yieldk(lua_State* L, int nrets, void* ctx, auto_lua_KFunction k)
{
    return lua_yieldk(L, nrets, (lua_KContext)ctx, (lua_KFunction)(void*)k);
}

static void _lua_callk(lua_State* L, int nargs, int nrets, void* ctx, auto_lua_KFunction k)
{
    lua_callk(L, nargs, nrets, (lua_KContext)ctx, (lua_KFunction)(void*)k);
}

static int _lua_getglobal(lua_State* L, const char* name)
{
    return lua_getglobal(L, name);
}

static int _lua_compare(lua_State* L, int idx1, int idx2, int op)
{
    static_assert(AUTO_LUA_OPEQ == LUA_OPEQ, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_OPLE == LUA_OPLE, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_OPLT == LUA_OPLT, ERR_HINT_DEFINITION_MISMATCH);
    return lua_compare(L, idx1, idx2, op);
}

static void _lua_newtable(lua_State* L)
{
    lua_newtable(L);
}

static int _lua_getfield(lua_State* L, int idx, const char* k)
{
    return lua_getfield(L, idx, k);
}

static int _lua_geti(lua_State* L, int idx, int64_t i)
{
    return lua_geti(L, idx, i);
}

static int _lua_gettable(lua_State* L, int idx)
{
    return lua_gettable(L, idx);
}

static int _lua_gettop(lua_State* L)
{
    return lua_gettop(L);
}

static void _lua_insert(lua_State* L, int idx)
{
    lua_insert(L, idx);
}

static int _lua_next(lua_State* L, int idx)
{
    return lua_next(L, idx);
}

static void _lua_pop(lua_State* L, int n)
{
    lua_pop(L, n);
}

static void _lua_pushboolean(lua_State* L, int b)
{
    lua_pushboolean(L, b);
}

static void _lua_pushcclosure(lua_State* L, auto_lua_CFunction fn, int n)
{
    lua_pushcclosure(L, fn, n);
}

static void _lua_pushcfunction(lua_State* L, auto_lua_CFunction f)
{
    lua_pushcfunction(L, f);
}

static const char* _lua_pushfstring(lua_State* L, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char* ret = lua_pushvfstring(L, fmt, ap);
    va_end(ap);
    return ret;
}

static void _lua_pushinteger(lua_State* L, int64_t n)
{
    lua_pushinteger(L, n);
}

static void _lua_pushlightuserdata(lua_State* L, void* p)
{
    lua_pushlightuserdata(L, p);
}

static const char* _lua_pushlstring(lua_State* L, const char* s, size_t len)
{
    return lua_pushlstring(L, s, len);
}

static void _lua_pushnil(lua_State* L)
{
    lua_pushnil(L);
}

static void _lua_pushnumber(lua_State* L, double n)
{
    lua_pushnumber(L, n);
}

static const char* _lua_pushstring(lua_State* L, const char* s)
{
    return lua_pushstring(L, s);
}

static void _lua_pushvalue(lua_State* L, int idx)
{
    lua_pushvalue(L, idx);
}

static void _lua_remove(lua_State* L, int idx)
{
    lua_remove(L, idx);
}

static void _lua_replace(lua_State* L, int idx)
{
    lua_replace(L, idx);
}

static void _lua_rotate(lua_State* L, int idx, int n)
{
    lua_rotate(L, idx, n);
}

static void _lua_setfield(lua_State* L, int idx, const char* k)
{
    lua_setfield(L, idx, k);
}

static void _lua_setglobal(lua_State* L, const char* name)
{
    lua_setglobal(L, name);
}

static void _lua_seti(lua_State* L, int idx, int64_t n)
{
    lua_seti(L, idx, n);
}

static void _lua_settable(lua_State* L, int idx)
{
    lua_settable(L, idx);
}

static int _lua_toboolean(lua_State* L, int idx)
{
    return lua_toboolean(L, idx);
}

static auto_lua_CFunction _lua_tocfunction(lua_State* L, int idx)
{
    return lua_tocfunction(L, idx);
}

static int64_t _lua_tointeger(lua_State* L, int idx)
{
    return lua_tointeger(L, idx);
}

static const char* _lua_tolstring(lua_State* L, int idx, size_t* len)
{
    return lua_tolstring(L, idx, len);
}

static double _lua_tonumber(lua_State* L, int idx)
{
    return lua_tonumber(L, idx);
}

static const char* _lua_tostring(lua_State* L, int idx)
{
    return lua_tostring(L, idx);
}

static void* _lua_touserdata(lua_State* L, int idx)
{
    return lua_touserdata(L, idx);
}

static int _lua_type(lua_State* L, int idx)
{
    static_assert(AUTO_LUA_TNONE == LUA_TNONE, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TNIL == LUA_TNIL, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TNUMBER == LUA_TNUMBER, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TBOOLEAN == LUA_TBOOLEAN, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TSTRING == LUA_TSTRING, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TTABLE == LUA_TTABLE, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TFUNCTION == LUA_TFUNCTION, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TUSERDATA == LUA_TUSERDATA, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TTHREAD == LUA_TTHREAD, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_TLIGHTUSERDATA == LUA_TLIGHTUSERDATA, ERR_HINT_DEFINITION_MISMATCH);
    return lua_type(L, idx);
}

static const char* _lua_L_typename(lua_State* L, int tp)
{
    return luaL_typename(L, tp);
}

static int _lua_L_ref(lua_State* L, int t)
{
    static_assert(AUTO_LUA_REGISTRYINDEX == LUA_REGISTRYINDEX, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_REFNIL == LUA_REFNIL, ERR_HINT_DEFINITION_MISMATCH);
    static_assert(AUTO_LUA_NOREF == LUA_NOREF, ERR_HINT_DEFINITION_MISMATCH);
    return luaL_ref(L, t);
}

static void _lua_L_unref(lua_State* L, int t, int ref)
{
    luaL_unref(L, t, ref);
}

static const char* _lua_L_checklstring(lua_State* L, int arg, size_t *l)
{
    return luaL_checklstring(L, arg, l);
}

static void _lua_L_checktype(lua_State* L, int arg, int t)
{
    luaL_checktype(L, arg, t);
}

static void _lua_L_checkudata(lua_State* L, int arg, const char* tname)
{
    luaL_checkudata(L, arg, tname);
}

static const char* _lua_pushvfstring(lua_State* L, const char* fmt, va_list argp)
{
    return lua_pushvfstring(L, fmt, argp);
}

static double _lua_L_checknumber(lua_State* L, int arg)
{
    return luaL_checknumber(L, arg);
}

static int64_t _lua_L_checkinteger(lua_State* L, int arg)
{
    return luaL_checkinteger(L, arg);
}

static void* _lua_newuserdatauv(lua_State* L, size_t sz, int nuvalue)
{
    return lua_newuserdatauv(L, sz, nuvalue);
}

static int _lua_setiuservalue(lua_State* L, int idx, int n)
{
    return lua_setiuservalue(L, idx, n);
}

static int _lua_getiuservalue(lua_State* L, int idx, int n)
{
    return lua_getiuservalue(L, idx, n);
}

static int64_t _lua_L_len(lua_State* L, int idx)
{
    return luaL_len(L, idx);
}

static void _lua_L_newlib(lua_State* L, const auto_luaL_Reg l[])
{
    static_assert(sizeof(auto_luaL_Reg) == sizeof(luaL_Reg), ERR_HINT_DEFINITION_MISMATCH);
    static_assert(offsetof(auto_luaL_Reg, name) == offsetof(luaL_Reg, name), ERR_HINT_DEFINITION_MISMATCH);
    static_assert(offsetof(auto_luaL_Reg, func) == offsetof(luaL_Reg, func), ERR_HINT_DEFINITION_MISMATCH);
    lua_newtable(L);
    luaL_setfuncs(L, (luaL_Reg*)l, 0);
}

static const char* _lua_L_checkstring(lua_State* L, int arg)
{
    return luaL_checkstring(L, arg);
}

static int _lua_L_newmetatable(lua_State* L, const char* tname)
{
    return luaL_newmetatable(L, tname);
}

static int _lua_setmetatable(lua_State* L, int idx)
{
    return lua_setmetatable(L, idx);
}

static void _lua_L_setfuncs(lua_State* L, const auto_luaL_Reg* l, int nup)
{
    luaL_setfuncs(L, (luaL_Reg*)l, nup);
}

static void _lua_settop(lua_State* L, int idx)
{
    lua_settop(L, idx);
}

static int _lua_isyieldable(lua_State* L)
{
    return lua_isyieldable(L);
}

static void _lua_concat(lua_State* L, int n)
{
    lua_concat(L, n);
}

static lua_State* _lua_newthread(lua_State* L)
{
    return lua_newthread(L);
}

static int _lua_rawgeti(lua_State* L, int idx, int64_t n)
{
    return lua_rawgeti(L, idx, n);
}

static const char* _lua_L_gsub(lua_State* L, const char* s, const char* p, const char* r)
{
    return luaL_gsub(L, s, p, r);
}

static int _lua_A_pushverror(struct lua_State* L, const char *fmt, va_list ap)
{
    cJSON* err_obj = cJSON_CreateObject();

    /* Message */
    {
        lua_pushvfstring(L, fmt, ap);
        cJSON_AddStringToObject(err_obj, "message", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    /* Traceback */
    {
        luaL_traceback(L, L, NULL, 1);
        cJSON_AddStringToObject(err_obj, "traceback", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    char* err_obj_str = cJSON_PrintUnformatted(err_obj);
    lua_pushstring(L, err_obj_str);
    cJSON_free(err_obj_str);

    return 1;
}

static int _lua_A_error(struct lua_State* L, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _lua_A_pushverror(L, fmt, ap);
    va_end(ap);

    return lua_error(L);
}

static int _lua_A_pusherror(struct lua_State* L, const char *fmt, ...)
{
    int ret;

    va_list ap;
    va_start(ap, fmt);
    ret = _lua_A_pushverror(L, fmt, ap);
    va_end(ap);

    return ret;
}

const auto_api_lua_t api_lua = {
    _lua_callk,
    _lua_compare,
    _lua_concat,
    _lua_getfield,
    _lua_getglobal,
    _lua_geti,
    _lua_getiuservalue,
    _lua_gettable,
    _lua_gettop,
    _lua_insert,
    _lua_isyieldable,
    _lua_newtable,
    _lua_newthread,
    _lua_newuserdatauv,
    _lua_next,
    _lua_pop,
    _lua_pushboolean,
    _lua_pushcclosure,
    _lua_pushcfunction,
    _lua_pushfstring,
    _lua_pushinteger,
    _lua_pushlightuserdata,
    _lua_pushlstring,
    _lua_pushnil,
    _lua_pushnumber,
    _lua_pushstring,
    _lua_pushvalue,
    _lua_pushvfstring,
    _lua_rawgeti,
    _lua_remove,
    _lua_replace,
    _lua_rotate,
    _lua_setfield,
    _lua_setglobal,
    _lua_seti,
    _lua_setiuservalue,
    _lua_setmetatable,
    _lua_settable,
    _lua_settop,
    _lua_toboolean,
    _lua_tocfunction,
    _lua_tointeger,
    _lua_tolstring,
    _lua_tonumber,
    _lua_tostring,
    _lua_touserdata,
    _lua_type,
    _lua_yieldk,
    _lua_A_callk,
    _lua_A_error,
    _lua_A_pusherror,
    _lua_A_pushverror,
    _lua_L_checkinteger,
    _lua_L_checklstring,
    _lua_L_checknumber,
    _lua_L_checkstring,
    _lua_L_checktype,
    _lua_L_checkudata,
    _lua_L_gsub,
    _lua_L_len,
    _lua_L_newlib,
    _lua_L_newmetatable,
    _lua_L_ref,
    _lua_L_setfuncs,
    _lua_L_typename,
    _lua_L_unref,
};
