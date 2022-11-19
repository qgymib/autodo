#include "lua/string.h"
#include <assert.h>

typedef struct string_split_helper
{
    lua_State*  L;
    const char* str;
    size_t      offset;
} string_split_helper_t;

static void _string_split_cb(const char* data, size_t* groups, size_t group_sz, void* arg)
{
    string_split_helper_t* helper = arg;
    lua_State* L = helper->L;

    assert(group_sz >= 1); (void)group_sz;

    size_t pos_beg = groups[0];
    size_t pos_end = groups[1];

    const char* str_beg = data + helper->offset;
    size_t str_sz = pos_beg - helper->offset;

    /* Skip empty string */
    if (str_sz > 0)
    {
        lua_pushlstring(L, str_beg, str_sz);
        lua_rawseti(L, -2, luaL_len(L, -2) + 1);
    }

    helper->offset = pos_end;
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
    helper.offset = 0;

    lua_newtable(L);
    while (helper.offset < str_sz)
    {
        if (api.regex->match(code, str, str_sz, helper.offset, _string_split_cb, &helper) <= 0)
        {
            break;
        }
    }

    /* The last string. */
    if (helper.offset < str_sz)
    {
        const char* left_str_beg = str + helper.offset;
        size_t left_str_sz = str_sz - helper.offset;
        lua_pushlstring(L, left_str_beg, left_str_sz);
        lua_rawseti(L, -2, luaL_len(L, -2) + 1);
    }

    api.regex->destroy(code);

    return 1;
}
