#include "package.h"
#include "runtime.h"
#include "utils.h"
#include <stdlib.h>

static int _package_download_and_load(lua_State* L, const char* package_name)
{
    /* Don't download abs path file */
    if (atd_isabs(package_name))
    {
        lua_pushfstring(L, "cannot load %s", package_name);
        return 1;
    }

    return 0;
}

int atd_package_loader(lua_State* L)
{
    int ret;

    atd_runtime_t* rt = auto_get_runtime(L);
    int sp = lua_gettop(L);

    const char* package_name = lua_tostring(L, 1);
    const char* filename = get_filename(package_name);

#if defined(_WIN32)
    const char* ext = "dll";
#else
    const char* ext = "so";
#endif

    /* sp + 1: Generate load load_path */
    const char* load_path;
    if (atd_isabs(package_name))
    {
        load_path = lua_pushfstring(L, "%s", package_name);
    }
    else
    {
        load_path = lua_pushfstring(L, "%s/%s.%s",
            rt->config.script_path, package_name, ext);
    }

    uv_lib_t lib;
    if ((ret = uv_dlopen(load_path, &lib)) != 0)
    {/* Open failed */
        uv_dlclose(&lib);
        lua_settop(L, sp);
        return _package_download_and_load(L, package_name);
    }

    /* sp + 2: Get load function */
    const char* func_name = lua_pushfstring(L, "luaopen_%s", filename);
    lua_CFunction entrypoint = NULL;
    if ((ret = uv_dlsym(&lib, func_name, (void**)&entrypoint)) != 0)
    {
        uv_dlclose(&lib);
        lua_pushfstring(L, "%s", uv_dlerror(&lib));
        return 1;
    }

    /* Found */
    lua_pushcfunction(L, entrypoint);
    lua_pushstring(L, load_path);

    return 2;
}
