#include <autodo.h>
#include <string.h>
#include <stdlib.h>
#include "process.h"
#include "utils.h"
#include "utils/list.h"

typedef struct lua_process_cache
{
    ev_list_node_t      node;
    size_t              size;
    char                data[];
} lua_process_cache_t;

typedef struct lua_process
{
    atd_process_t*      process;
    atd_coroutine_t*    host_co;
    atd_process_cfg_t   cfg;

    struct
    {
        int             have_stdin;
        int             have_stdout;
        int             have_stderr;
        int             have_error;
    } flag;

    struct
    {
        ev_list_t       data_stdout;
        ev_list_t       data_stderr;
    } cache;
} lua_process_t;

static void _lua_process_stdout(atd_process_t* proc, void* data,
    size_t size, int status, void* arg)
{
    (void)proc;
    lua_process_t* process = arg;

    /* Wakeup when error */
    if (status < 0)
    {
        process->flag.have_error = 1;
        goto wakeup_host_co;
    }

    size_t malloc_size = sizeof(lua_process_cache_t) + size;
    lua_process_cache_t* cache = malloc(malloc_size);
    cache->size = size;
    memcpy(cache->data, data, size);

    ev_list_push_back(&process->cache.data_stdout, &cache->node);

wakeup_host_co:
    process->host_co->set_schedule_state(process->host_co, LUA_TNONE);
}

static void _lua_process_stderr(atd_process_t* proc, void* data,
    size_t size, int status, void* arg)
{
    (void)proc;
    lua_process_t* process = arg;

    if (status < 0)
    {
        process->flag.have_error = 1;
        goto wakeup_host_co;
    }

    size_t malloc_size = sizeof(lua_process_cache_t) + size;
    lua_process_cache_t* cache = malloc(malloc_size);
    cache->size = size;
    memcpy(cache->data, data, size);

    ev_list_push_back(&process->cache.data_stderr, &cache->node);

wakeup_host_co:
    process->host_co->set_schedule_state(process->host_co, LUA_TNONE);
}

static int _lua_process_table_to_cfg(lua_State* L, int idx, lua_process_t* process)
{
    size_t i;
    int sp = lua_gettop(L);
    process->cfg.arg = process;

    /* SP + 1 */
    if (lua_getfield(L, idx, "path") != LUA_TSTRING)
    {
        return luaL_error(L, "missing field `path`");
    }
    process->cfg.path = atd_strdup(lua_tostring(L, sp + 1));
    lua_pop(L, 1);

    /* sp + 1 */
    if (lua_getfield(L, idx, "cwd") == LUA_TSTRING)
    {
        process->cfg.cwd = atd_strdup(lua_tostring(L, sp + 1));
    }
    lua_pop(L, 1);

    /* sp + 1 */
    if (lua_getfield(L, idx, "args") == LUA_TTABLE)
    {
        size_t arg_len = luaL_len(L, sp + 1);
        process->cfg.args = malloc(sizeof(char*) * (arg_len + 1));
        process->cfg.args[arg_len] = NULL;

        for (i = 0; i < arg_len; i++)
        {
            lua_geti(L, sp + 1, i + 1); // sp + 2
            process->cfg.args[i] = atd_strdup(lua_tostring(L, sp + 2));
            lua_pop(L, 1);
        }
    }
    else
    {
        process->cfg.args = malloc(sizeof(char*) * 2);
        process->cfg.args[0] = atd_strdup(process->cfg.path);
        process->cfg.args[1] = NULL;
    }
    lua_pop(L, 1);

    /* sp + 1 */
    if (lua_getfield(L, idx, "envs") == LUA_TTABLE)
    {
        size_t env_len = luaL_len(L, sp + 1);
        process->cfg.envs = malloc(sizeof(char*) * (env_len + 1));
        process->cfg.envs[env_len] = NULL;

        for (i = 0; i < env_len; i++)
        {
            lua_geti(L, sp + 1, i + 1); // sp + 2
            process->cfg.envs[i] = atd_strdup(lua_tostring(L, sp + 2));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    /* sp + 1 */
    if (lua_getfield(L, idx, "stdio") == LUA_TTABLE)
    {
        lua_pushnil(L);
        while (lua_next(L, sp + 1) != 0)
        {
            const char* value = luaL_checkstring(L, -1);
            if (strcmp(value, "enable_stdin") == 0)
            {
                process->flag.have_stdin = 1;
            }
            else if (strcmp(value, "enable_stdout") == 0)
            {
                process->flag.have_stdout = 1;
            }
            else if (strcmp(value, "enable_stderr") == 0)
            {
                process->flag.have_stderr = 1;
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    if (process->flag.have_stdout)
    {
        process->cfg.stdout_fn = _lua_process_stdout;
    }

    if (process->flag.have_stderr)
    {
        process->cfg.stderr_fn = _lua_process_stderr;
    }

    return 0;
}

static int _lua_process_gc(lua_State *L)
{
    size_t i;
    lua_process_t* process = lua_touserdata(L, 1);

    if (process->process != NULL)
    {
        process->process->kill(process->process, 9);
        process->process = NULL;
    }

    if (process->cfg.path != NULL)
    {
        free((char*)process->cfg.path);
        process->cfg.path = NULL;
    }
    if (process->cfg.cwd != NULL)
    {
        free((char*)process->cfg.cwd);
        process->cfg.cwd = NULL;
    }
    if (process->cfg.args != NULL)
    {
        for (i = 0; process->cfg.args[i] != NULL; i++)
        {
            free(process->cfg.args[i]);
            process->cfg.args[i] = NULL;
        }
        free(process->cfg.args);
        process->cfg.args = NULL;
    }
    if (process->cfg.envs != NULL)
    {
        for (i = 0; process->cfg.envs[i] != NULL; i++)
        {
            free(process->cfg.envs[i]);
            process->cfg.envs[i] = NULL;
        }
        free(process->cfg.envs);
        process->cfg.envs = NULL;
    }

    return 0;
}

static int _lua_process_kill(lua_State *L)
{
    lua_process_t* process = lua_touserdata(L, 1);
    int signum = lua_tointeger(L, 2);

    if (process->process != NULL)
    {
        process->process->kill(process->process, signum);
        process->process = NULL;
    }

    return 0;
}

static int _lua_process_on_stdout_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)status;
    ev_list_node_t* it;
    lua_process_t* process = (lua_process_t*)ctx;

    if (process->flag.have_error)
    {
        return 0;
    }

    if (ev_list_size(&process->cache.data_stdout) == 0)
    {
        process->host_co->set_schedule_state(process->host_co, LUA_YIELD);
        return lua_yieldk(L, 0, (lua_KContext)process,
            _lua_process_on_stdout_resume);
    }

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    while ((it = ev_list_pop_front(&process->cache.data_stdout)) != NULL)
    {
        lua_process_cache_t* cache = container_of(it, lua_process_cache_t, node);
        luaL_addlstring(&buf, cache->data, cache->size);
        free(cache);
    }

    luaL_pushresult(&buf);
    return 1;
}

static int _lua_process_on_stderr_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)status;
    ev_list_node_t* it;
    lua_process_t* process = (lua_process_t*)ctx;

    if (process->flag.have_error)
    {
        return 0;
    }

    if (ev_list_size(&process->cache.data_stderr) == 0)
    {
        process->host_co->set_schedule_state(process->host_co, LUA_YIELD);
        return lua_yieldk(L, 0, (lua_KContext)process,
            _lua_process_on_stderr_resume);
    }

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    while ((it = ev_list_pop_front(&process->cache.data_stderr)) != NULL)
    {
        lua_process_cache_t* cache = container_of(it, lua_process_cache_t, node);
        luaL_addlstring(&buf, cache->data, cache->size);
        free(cache);
    }

    luaL_pushresult(&buf);
    return 1;
}

static int _lua_process_await_stdout(lua_State *L)
{
    lua_process_t* process = lua_touserdata(L, 1);
    if (!process->flag.have_stdout)
    {
        return luaL_error(L, "stdout have been disabled");
    }

    return _lua_process_on_stdout_resume(L, LUA_YIELD, (lua_KContext)process);
}

static int _lua_process_await_stderr(lua_State *L)
{
    lua_process_t* process = lua_touserdata(L, 1);

    if (!process->flag.have_stderr)
    {
        return luaL_error(L, "stderr have been disabled");
    }

    return _lua_process_on_stderr_resume(L, LUA_YIELD, (lua_KContext)process);
}

int atd_lua_process(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_process_t* process = lua_newuserdata(L, sizeof(lua_process_t));
    memset(process, 0, sizeof(*process));

    process->host_co = api.find_coroutine(L);
    ev_list_init(&process->cache.data_stdout);
    ev_list_init(&process->cache.data_stderr);

    static const luaL_Reg s_process_meta[] = {
        { "__gc",           _lua_process_gc },
        { NULL,             NULL },
    };
    static const luaL_Reg s_process_method[] = {
        { "kill",           _lua_process_kill },
        { "await_stdout",   _lua_process_await_stdout },
        { "await_stderr",   _lua_process_await_stderr },
        { NULL,             NULL },
    };
    if (luaL_newmetatable(L, "__auto_process") != 0)
    {
        luaL_setfuncs(L, s_process_meta, 0);
        luaL_newlib(L, s_process_method);
        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);

    _lua_process_table_to_cfg(L, 1, process);

    if ((process->process = api.new_process(&process->cfg)) == NULL)
    {
        lua_pop(L, 1);
        return 0;
    }

    return 1;
}
