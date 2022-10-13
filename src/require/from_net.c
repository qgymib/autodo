#include "from_net.h"
#include "runtime.h"
#include <string.h>
#include <cJSON.h>

/**
 * @brief
 * @param L
 * @param url
 * @param dst
 * @return      Boolean.
 */
static int _download_from_internet(lua_State* L, const char* url, const char* file)
{
    int ret = 0;
    int sp = lua_gettop(L);
    lua_getglobal(L, "os");

    {
        lua_getfield(L, -1, "execute");
        lua_pushfstring(L, "curl %s --output %s", url, file);
        lua_pcall(L, 1, LUA_MULTRET, 0);
    }

    lua_settop(L, sp);
    return ret;
}

static int _net_loader_from_url(lua_State* L, const char* url, const char* name)
{

}

static int _net_loader_from_path(lua_State* L, const char* path, const char* name)
{

}

static int _net_loader(lua_State* L)
{
    int ret;
    const char* module_string = lua_tostring(L, 1);
    auto_lua_module_t* module = lua_touserdata(L, 2);

    char *url, *name; cJSON* opt;
    auto_require_split(module_string, &url, &name, &opt);

    cJSON* url_json = (opt != NULL) ? cJSON_GetObjectItem(opt, "url") : NULL;
    if (url_json != NULL && url_json->type == cJSON_String)
    {
        ret = _net_loader_from_url(L, url_json->valuestring, name);
    }
    else
    {
        ret = _net_loader_from_path(L, url, name);
    }

    free(url);
    free(name);
    cJSON_Delete(opt);

    return ret;
}

int auto_load_net_module(lua_State* L, auto_lua_module_t* module)
{
    /* Get argument of `require()` */
    const char* module_name = lua_tostring(L, 1);

    /*
     * Searchers should raise no errors and have no side effects in Lua.
     * So it is better to use an independent vm.
     */
    lua_State* tmp_vm = luaL_newstate();

    /* Initialize basic runtime. */
    luaL_openlibs(tmp_vm);
    auto_init_runtime(tmp_vm);
    auto_init_libs(tmp_vm);

    /* Do download */
    lua_pushcfunction(tmp_vm, _net_loader);
    lua_pushstring(tmp_vm, module_name);
    lua_pushlightuserdata(tmp_vm, module);
    int ret = lua_pcall(tmp_vm, 2, 1, 0);
    ret = (ret == LUA_OK) ? lua_toboolean(tmp_vm, -1) : 0;

    /* Cleanup. */
    lua_close(tmp_vm);

    return ret;
}
