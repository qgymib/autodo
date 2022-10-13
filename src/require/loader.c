#include "loader.h"
#include "from_file.h"
#include "from_net.h"
#include "runtime.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Package downloader and loader.
 * @param[in] L     Lua VM.
 * @return
 */
static int _custom_package_loader(lua_State* L)
{
    atd_runtime_t* rt = auto_get_runtime(L);

    auto_lua_module_t* module = malloc(sizeof(auto_lua_module_t));
    memset(module, 0, sizeof(auto_lua_module_t));

    /* First try load from local filesystem. */
    if (auto_load_local_module(L, module))
    {
        goto finish;
    }

    /* Secondly download from net. */
    if (auto_load_net_module(L, module))
    {
        goto finish;
    }

    /* Release resources. */
    free(module);

    /* Push error string. */
    lua_pushfstring(L, "cannot load %s", lua_tostring(L, 1));
    return 1;

finish:
    ev_list_push_back(&rt->cache.modules, &module->node);
    lua_pushcfunction(L, module->data.entry);
    lua_pushstring(L, module->data.path);
    return 2;
}

int auto_inject_loader(lua_State* L)
{
    int sp = lua_gettop(L);

    /* sp + 1 */
    if (lua_getglobal(L, "package") != LUA_TTABLE)
    {
        return 0;
    }
    /* sp + 2 */
    if (lua_getfield(L, sp + 1, "searchers") != LUA_TTABLE)
    {
        lua_pop(L, 1);
        return 0;
    }

    /* Append custom loader to the end of searchers table */
    lua_pushcfunction(L, _custom_package_loader);
    size_t len = luaL_len(L, sp + 2);
    lua_seti(L, sp + 2, len + 1);

    /* Resource stack */
    lua_settop(L, sp);

    return 1;
}

void auto_require_split(const char* raw, char** path, char** name, cJSON** opt)
{
    const char* last_name = get_filename(raw);

    /* Get path */
    if (last_name != raw)
    {
        size_t len = last_name - raw + 1;
        *path = malloc(len);
        memcpy(*path, raw, len - 1);
        (*path)[len] = '\0';
    }
    else
    {
        *path = NULL;
    }

    const char* split_pos = strstr(last_name, ":");
    if (split_pos != NULL)
    {
        size_t len = split_pos - last_name + 1;
        *name = malloc(len);
        memcpy(*name, last_name, len - 1);
        (*name)[len] = '\0';

        *opt = cJSON_Parse(split_pos + 1);
    }
    else
    {
        *name = strdup(last_name);
        *opt = NULL;
    }
}
