#define _GNU_SOURCE
#include "download.h"
#include "utils.h"
#include <string.h>

typedef void (*command_merger_fn)(lua_State* L, const char* url, const char* file);

typedef struct lua_download_token
{
    int     process_ref;    /**< Reference key to process token */
    char*   url;            /**< No need to free. */
    char*   file;           /**< No need to free. */
} lua_download_token_t;

static int _downloader_gc(lua_State *L)
{
    lua_download_token_t* token = lua_touserdata(L, 1);

    if (token->process_ref != LUA_NOREF)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, token->process_ref);
        token->process_ref = LUA_NOREF;
    }

    return 0;
}

static void _download_with_curl(lua_State* L, const char* url, const char* file)
{
    size_t i;
    const char* cmd_list[] = { "curl", url, "--output", file };
    for (i = 0; i < ARRAY_SIZE(cmd_list); i++)
    {
        lua_pushstring(L, cmd_list[i]);
        lua_seti(L, -2, i + 1);
    }
}

static void _download_with_wget(lua_State* L, const char* url, const char* file)
{
    size_t i;
    const char* cmd_list[] = { "wget", "-O", file, url };
    for (i = 0; i < ARRAY_SIZE(cmd_list); i++)
    {
        lua_pushstring(L, cmd_list[i]);
        lua_seti(L, -2, i + 1);
    }
}

static void _download_with_powershell(lua_State* L, const char* url, const char* file)
{
    char* cmd;
    asprintf(&cmd, "Invoke-WebRequest -Uri \"%s\" -OutFile \"%s\"", url, file);

    size_t i;
    const char* cmd_list[] = { "PowerShell", "-Command", cmd };
    for (i = 0; i < ARRAY_SIZE(cmd_list); i++)
    {
        lua_pushstring(L, cmd_list[i]);
        lua_seti(L, -2, i + 1);
    }

    free(cmd);
}

static int _download_with_command(lua_State* L, lua_download_token_t* token,
    const char* url, const char* file, command_merger_fn merger)
{
    int sp = lua_gettop(L);

    lua_getglobal(L, "auto");
    lua_getfield(L, -1, "process");

    lua_newtable(L);
    lua_newtable(L);
    merger(L, url, file);
    lua_setfield(L, -2, "args");
    lua_call(L, 1, 1);

    if (lua_type(L, -1) != LUA_TNIL)
    {
        token->process_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_settop(L, sp);
    return token->process_ref != LUA_NOREF;
}

/**
 * @brief Download \p url with installed tool in path.
 * @param[in] L         Lua VM.
 * @param[out] token    Download process token.
 * @param[in] url       File url.
 * @param[in] file      Filesystem position to store the file.
 * @return              Boolean.
 */
static int _download_with_installed_tool(lua_State* L, lua_download_token_t* token,
    const char* url, const char* file)
{
    static command_merger_fn s_merger_list[] = {
        _download_with_wget,
        _download_with_curl,
        _download_with_powershell,
    };

    size_t i;
    for (i = 0; i < ARRAY_SIZE(s_merger_list); i++)
    {
        if (_download_with_command(L, token, url, file, s_merger_list[i]))
        {
            return 1;
        }
    }

    return 0;
}

static int _download_on_wait_resume(lua_State* L, int status, lua_KContext ctx)
{
    (void)status; (void)ctx;

    int exit_code = lua_tointeger(L, -1);
    lua_pushinteger(L, exit_code);
    return 1;
}

static int _download_wait_for_finish(lua_State *L)
{
    lua_download_token_t* token = lua_touserdata(L, 1);

    lua_rawgeti(L, LUA_REGISTRYINDEX, token->process_ref);
    lua_getfield(L, -1, "join");
    lua_pushvalue(L, -2);

    lua_callk(L, 1, 1, (lua_KContext)token, _download_on_wait_resume);
    return _download_on_wait_resume(L, LUA_YIELD, (lua_KContext)token);
}

static void _download_set_metatable(lua_State* L)
{
    static const luaL_Reg s_downloader_meta[] = {
        { "__gc",       _downloader_gc },
        { NULL,         NULL },
    };
    static const luaL_Reg s_downloader_method[] = {
        { "wait",       _download_wait_for_finish },
        { NULL,         NULL },
    };
    if (luaL_newmetatable(L, "__auto_download") != 0)
    {
        luaL_setfuncs(L, s_downloader_meta, 0);
        luaL_newlib(L, s_downloader_method);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

int auto_lua_download(lua_State* L)
{
    size_t url_size;
    const char* url = luaL_checklstring(L, 1, &url_size);

    size_t file_size;
    const char* file = luaL_checklstring(L, 2, &file_size);

    size_t data_size = sizeof(lua_download_token_t) + url_size + file_size + 2;
    lua_download_token_t* token = lua_newuserdata(L, data_size);
    memset(token, 0, sizeof(*token));
    token->process_ref = LUA_NOREF;
    token->url = (char*)(token + 1);
    token->file = token->url + url_size + 1;
    memcpy(token->url, url, url_size + 1);
    memcpy(token->file, file, file_size + 1);

    _download_set_metatable(L);

    if (_download_with_installed_tool(L, token, url, file))
    {
        return 1;
    }

    lua_pop(L, 1);
    return 0;
}
