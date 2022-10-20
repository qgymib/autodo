/**
 * This macro must be defined before including pcre2.h. For a program that uses
 * only one code unit width, it makes it possible to use generic function names
 * such as pcre2_compile().
 */
#define PCRE2_CODE_UNIT_WIDTH 8
/**
 * Statically link PCRE2
 */
#define PCRE2_STATIC
#include <pcre2.h>
#include <string.h>
#include "regex.h"

typedef struct lua_regex
{
    pcre2_code* pattern;
} lua_regex_t;

static int _regex_gc(lua_State* L)
{
    lua_regex_t* self = lua_touserdata(L, 1);

    if (self->pattern != NULL)
    {
        pcre2_code_free(self->pattern);
        self->pattern = NULL;
    }

    return 0;
}

static int _regex_match_internal(lua_State* L, lua_regex_t* self, const char* str, size_t len,
    pcre2_match_data* match_data)
{
    int rc = pcre2_match(self->pattern, (PCRE2_SPTR)str, len, 0, 0, match_data, NULL);

    /* Match failed */
    if (rc == PCRE2_ERROR_NOMATCH)
    {
        lua_pushboolean(L, 0);
        lua_pushnil(L);
        return 2;
    }

    if (rc <= 0)
    {/* Which should be never happen */
        return luaL_error(L, "regex: pcre2_match returns %d", rc);
    }

    lua_pushboolean(L, 1);
    lua_newtable(L);

    /*
     * For the `nth` group, `groups[2n]` is the offset of start substring,
     * `groups[2n+1]` is the offset of end substring.
     */
    PCRE2_SIZE* groups = pcre2_get_ovector_pointer(match_data);

    int i;
    for (i = 0; i < rc; i++)
    {
        size_t substring_len = groups[2 * i + 1] - groups[2 * i];
        lua_pushlstring(L, str + groups[2 * i], substring_len);
        lua_seti(L, -2, i + 1);
    }

    return 2;
}

static int _regex_match(lua_State* L)
{
    lua_regex_t* self = lua_touserdata(L, 1);

    size_t str_size;
    const char* str = luaL_checklstring(L, 2, &str_size);

    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(self->pattern, NULL);
    int ret = _regex_match_internal(L, self, str, str_size, match_data);
    pcre2_match_data_free(match_data);

    return ret;
}

static void _regex_set_metatable(lua_State* L)
{
    static const luaL_Reg s_regex_meta[] = {
            { "__gc",       _regex_gc },
            { NULL,         NULL },
    };
    static const luaL_Reg s_regex_method[] = {
            { "match",      _regex_match },
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

    int error_number;
    PCRE2_SIZE error_offset;
    self->pattern = pcre2_compile((PCRE2_SPTR)pattern, pattern_size, 0,
        &error_number, &error_offset, NULL);
    if (self->pattern == NULL)
    {
        lua_pop(L, 1);
        return 0;
    }

    return 1;
}

static auto_regex_code_t* _regex_api_compile(const char* pattern, size_t size)
{
    int error_number;
    PCRE2_SIZE error_offset;
    pcre2_code* code = pcre2_compile((PCRE2_SPTR)pattern, size, 0, &error_number,
        &error_offset, NULL);
    return (auto_regex_code_t*)code;
}

static void _regex_api_release(auto_regex_code_t* code)
{
    pcre2_code_free((pcre2_code*)code);
}

static size_t _regex_api_get_group_count(const auto_regex_code_t* code)
{
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern((pcre2_code*)code, NULL);
    size_t count = pcre2_get_ovector_count(match_data);
    pcre2_match_data_free(match_data);
    return count;
}

static int _regex_api_match(const auto_regex_code_t* code, const char* subject, size_t subject_len,
    size_t* groups, size_t group_len)
{
    pcre2_match_data* match_data = NULL;
    if (groups != NULL && group_len > 0)
    {
        match_data = pcre2_match_data_create_from_pattern((pcre2_code*)code, NULL);
    }

    int ret = pcre2_match((pcre2_code*)code, (PCRE2_SPTR)subject, subject_len, 0, 0, match_data, NULL);
    if (ret <= 0)
    {
        ret = -1;
        goto finish;
    }

    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    size_t ovector_len = pcre2_get_ovector_count(match_data);
    size_t copy_size = ovector_len < group_len ? ovector_len : group_len;

    size_t i;
    for (i = 0; i < copy_size; i++)
    {
        groups[2 * i] = ovector[2 * i];
        groups[2 * i + 1] = ovector[2 * i + 1];
    }

finish:
    if (match_data != NULL)
    {
        pcre2_match_data_free(match_data);
    }

    return ret;
}

const auto_api_regex_t api_regex = {
    _regex_api_compile,
    _regex_api_release,
    _regex_api_get_group_count,
    _regex_api_match,
};
