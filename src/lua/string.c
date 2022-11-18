#include "lua/string.h"
#include <assert.h>

typedef struct string_split_helper
{
    lua_State*  L;
    const char* str;
    size_t      str_sz;
} string_split_helper_t;

static void _string_split_match_cb(const char* data, size_t* groups, size_t group_sz, void* arg)
{
    string_split_helper_t* helper = arg;
    lua_State* L = helper->L;

    assert(group_sz >= 1); (void)group_sz;

    size_t pos_beg = groups[0];
    size_t pos_end = groups[1];

    /* Skip empty string */
    if (pos_beg > 0)
    {
        lua_pushlstring(L, data, pos_beg);
        lua_rawseti(L, -2, luaL_len(L, -2) + 1);
    }

    helper->str += pos_end;
    helper->str_sz -= pos_end;
}

int auto_lua_string_split(lua_State* L)
{
    size_t str_sz;
    const char* str = luaL_checklstring(L, 1, &str_sz);

    size_t pat_sz;
    const char* pat = luaL_checklstring(L, 2, &pat_sz);

    size_t err_pos;
    auto_regex_code_t* code = api.regex->create(pat, pat_sz, &err_pos);
    if (code == NULL)
    {
        return api.lua->A_error(L, "compile regex failed at position %d", (int)err_pos);
    }

    string_split_helper_t helper;
    helper.L = L;
    helper.str = str;
    helper.str_sz = str_sz;

    lua_newtable(L);
    while (api.regex->match(code, helper.str, helper.str_sz, _string_split_match_cb, &helper) > 0)
    {
        // do nothing
    }

    /* The last string. */
    lua_pushstring(L, helper.str);
    lua_rawseti(L, -2, luaL_len(L, -2) + 1);

    api.regex->destroy(code);

    return 1;
}
