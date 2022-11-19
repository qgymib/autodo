#include <string.h>
#include "regex.h"

typedef struct lua_regex
{
    auto_regex_code_t*  code;
} lua_regex_t;

static int _regex_lua_gc(lua_State* L)
{
    lua_regex_t* self = lua_touserdata(L, 1);

    if (self->code != NULL)
    {
        api.regex->destroy(self->code);
        self->code = NULL;
    }

    return 0;
}

static void _regex_match_cb(const char* subject, size_t* groups, size_t group_sz, void* arg)
{
    lua_State* L = arg;

    lua_newtable(L);

    size_t i;
    for (i = 0; i < group_sz; i++)
    {
        size_t pos_end = groups[2 * i + 1];
        size_t pos_beg = groups[2 * i];

        lua_newtable(L);

        /* Position */
        lua_pushnumber(L, (lua_Number)pos_beg);
        lua_rawseti(L, -2, 1);
        /* Length */
        lua_pushnumber(L, (lua_Number)(pos_end - pos_beg));
        lua_rawseti(L, -2, 2);
        /* Content */
        lua_pushlstring(L, subject + pos_beg, pos_end - pos_beg);
        lua_rawseti(L, -2, 3);

        lua_rawseti(L, -2, i + 1);
    }
}

static int _regex_lua_match(lua_State* L)
{
    lua_regex_t* self = lua_touserdata(L, 1);

    size_t str_size;
    const char* str = luaL_checklstring(L, 2, &str_size);

    size_t offset = 0;
    if (lua_type(L, 3) == LUA_TNUMBER)
    {
        lua_Integer tmp_offset = lua_tointeger(L, 3);
        if (tmp_offset > 0)
        {
            offset = (size_t)tmp_offset;
        }
    }

    if (api.regex->match(self->code, str, str_size, offset, _regex_match_cb, L) < 0)
    {
        lua_pushnil(L);
    }

    return 1;
}

static void _regex_set_metatable(lua_State* L)
{
    static const luaL_Reg s_regex_meta[] = {
        { "__gc",       _regex_lua_gc },
        { NULL,         NULL },
    };
    static const luaL_Reg s_regex_method[] = {
        { "match",      _regex_lua_match },
        { NULL,         NULL },
    };
    if (luaL_newmetatable(L, "__auto_regex") != 0)
    {
        luaL_setfuncs(L, s_regex_meta, 0);
        luaL_newlib(L, s_regex_method);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

int auto_lua_regex(lua_State* L)
{
    size_t pattern_size;
    const char* pattern = luaL_checklstring(L, 1, &pattern_size);

    lua_regex_t* self = lua_newuserdata(L, sizeof(lua_regex_t));
    memset(self, 0, sizeof(*self));
    _regex_set_metatable(L);

    size_t err_pos;
    if ((self->code = api.regex->create(pattern, pattern_size, &err_pos)) == NULL)
    {
        return api.lua->A_error(L, "compile regex failed at position %d", (int)err_pos);
    }

    return 1;
}
