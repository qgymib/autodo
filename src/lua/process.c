#include <autodo.h>
#include <string.h>
#include <stdlib.h>
#include "runtime.h"
#include "process.h"
#include "utils.h"
#include "utils/list.h"

typedef struct lua_process_cache
{
    std_list_node_t      node;
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
        atd_list_t       data_stdout;
        atd_list_t       data_stderr;
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
    api.coroutine.set_state(process->host_co, LUA_TNONE);
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
    api.coroutine.set_state(process->host_co, LUA_TNONE);
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
        api.process.kill(process->process, 9);
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
        api.process.kill(process->process, signum);
        process->process = NULL;
    }

    return 0;
}

static int _lua_process_on_stdout_resume(lua_State *L, int status, lua_KContext ctx)
{
    (void)status;
    std_list_node_t* it;
    lua_process_t* process = (lua_process_t*)ctx;

    if (process->flag.have_error)
    {
        return 0;
    }

    if (ev_list_size(&process->cache.data_stdout) == 0)
    {
        api.coroutine.set_state(process->host_co, LUA_YIELD);
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
    std_list_node_t* it;
    lua_process_t* process = (lua_process_t*)ctx;

    if (process->flag.have_error)
    {
        return 0;
    }

    if (ev_list_size(&process->cache.data_stderr) == 0)
    {
        api.coroutine.set_state(process->host_co, LUA_YIELD);
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
        return luaL_error(L, ERR_HINT_STDOUT_DISABLED);
    }

    return _lua_process_on_stdout_resume(L, LUA_YIELD, (lua_KContext)process);
}

static int _lua_process_await_stderr(lua_State *L)
{
    lua_process_t* process = lua_touserdata(L, 1);

    if (!process->flag.have_stderr)
    {
        return luaL_error(L, ERR_HINT_STDIN_DISABLED);
    }

    return _lua_process_on_stderr_resume(L, LUA_YIELD, (lua_KContext)process);
}

int atd_lua_process(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_process_t* process = lua_newuserdata(L, sizeof(lua_process_t));
    memset(process, 0, sizeof(*process));

    process->host_co = api.coroutine.find(L);
    if (process->host_co == NULL)
    {
        return luaL_error(L, ERR_HINT_NOT_IN_MANAGED_COROUTINE);
    }

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

    if ((process->process = api.process.create(&process->cfg)) == NULL)
    {
        lua_pop(L, 1);
        return 0;
    }

    return 1;
}

/******************************************************************************
* C API: .process
******************************************************************************/

struct atd_process_s
{
    uv_process_t            process;

    atd_process_stdio_fn    stdout_fn;
    atd_process_stdio_fn    stderr_fn;
    void*                   arg;

    int                     spawn_ret;
    int64_t                 exit_status;
    int                     term_signal;

    uv_pipe_t               pip_stdin;
    uv_pipe_t               pip_stdout;
    uv_pipe_t               pip_stderr;

    int                     flag_process_exit;
    int                     flag_stdin_exit;
    int                     flag_stdout_exit;
    int                     flag_stderr_exit;
};

typedef struct atd_process_write
{
    uv_write_t              req;
    atd_process_t*          impl;

    void*                   data;
    size_t                  size;

    atd_process_stdio_fn    cb;
    void*                   arg;
} atd_process_write_t;

static void _process_on_close(atd_process_t* impl)
{
    if (!impl->flag_process_exit || !impl->flag_stdin_exit || !impl->flag_stdout_exit || !impl->flag_stderr_exit)
    {
        return;
    }

    free(impl);
}

static void _process_on_process_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_process_t*)handle, atd_process_t, process);
    impl->flag_process_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdin_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stdin);
    impl->flag_stdin_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdout_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stdout);
    impl->flag_stdout_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stderr_close(uv_handle_t* handle)
{
    atd_process_t* impl = container_of((uv_pipe_t*)handle, atd_process_t, pip_stderr);
    impl->flag_stderr_exit = 1;
    _process_on_close(impl);
}

static void _process_write_done(uv_write_t *req, int status)
{
    atd_process_write_t* p_req = container_of(req, atd_process_write_t, req);
    p_req->cb(p_req->impl, p_req->data, p_req->size, status, p_req->arg);
    free(p_req);
}

static void _process_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    buf->len = suggested_size;
    buf->base = malloc(suggested_size);
}

static void _process_on_stderr(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_t* impl = container_of((uv_pipe_t*)stream, atd_process_t, pip_stderr);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stderr_fn(impl, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static void _process_on_stdout(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_t* impl = container_of((uv_pipe_t*)stream, atd_process_t, pip_stdout);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stdout_fn(impl, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static void _process_on_exit(uv_process_t* process, int64_t exit_status, int term_signal)
{
    atd_process_t* impl = container_of(process, atd_process_t, process);
    impl->exit_status = exit_status;
    impl->term_signal = term_signal;
}

atd_process_t* api_process_create(atd_process_cfg_t* cfg)
{
    atd_process_t* impl = malloc(sizeof(atd_process_t));
    memset(impl, 0, sizeof(*impl));

    impl->stdout_fn = cfg->stdout_fn;
    impl->stderr_fn = cfg->stderr_fn;
    impl->arg = cfg->arg;

    uv_stdio_container_t stdios[3];
    memset(stdios, 0, sizeof(stdios));

    uv_pipe_init(&g_rt->loop, &impl->pip_stdin, 0);
    stdios[0].flags = UV_CREATE_PIPE | UV_READABLE_PIPE;
    stdios[0].data.stream = (uv_stream_t*)&impl->pip_stdin;

    if (impl->stdout_fn != NULL)
    {
        uv_pipe_init(&g_rt->loop, &impl->pip_stdout, 0);
        stdios[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
        stdios[1].data.stream = (uv_stream_t*)&impl->pip_stdout;
    }
    else
    {
        impl->flag_stdout_exit = 1;
    }

    if (impl->stderr_fn != NULL)
    {
        uv_pipe_init(&g_rt->loop, &impl->pip_stderr, 0);
        stdios[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
        stdios[2].data.stream = (uv_stream_t*)&impl->pip_stderr;
    }
    else
    {
        impl->flag_stderr_exit = 1;
    }

    uv_process_options_t opt;
    memset(&opt, 0, sizeof(opt));
    opt.exit_cb = _process_on_exit;
    opt.file = cfg->path;
    opt.args = cfg->args;
    opt.env = cfg->envs;
    opt.cwd = cfg->cwd;
    opt.stdio = stdios;
    opt.stdio_count = 3;

    impl->spawn_ret = uv_spawn(&g_rt->loop, &impl->process, &opt);
    if (impl->spawn_ret != 0)
    {
        api.process.kill(impl, SIGKILL);
        return NULL;
    }

    uv_read_start((uv_stream_t*)&impl->pip_stdout, _process_alloc_cb,
        _process_on_stdout);
    uv_read_start((uv_stream_t*)&impl->pip_stderr, _process_alloc_cb,
        _process_on_stderr);

    return impl;
}

int api_process_send_to_stdin(atd_process_t* self, void* data,
    size_t size, atd_process_stdio_fn cb, void* arg)
{
    atd_process_write_t* p_req = malloc(sizeof(atd_process_write_t));
    p_req->impl = self;
    p_req->cb = cb;
    p_req->arg = arg;
    p_req->size = size;
    p_req->data = data;

    uv_buf_t buf = uv_buf_init(p_req->data, p_req->size);
    return uv_write(&p_req->req, (uv_stream_t*)&self->pip_stdin, &buf, 1,
                    _process_write_done);
}

void api_process_kill(atd_process_t* self, int signum)
{
    if (self->spawn_ret == 0)
    {
        uv_process_kill(&self->process, signum);
    }

    uv_close((uv_handle_t*)&self->process, _process_on_process_close);
    uv_close((uv_handle_t*)&self->pip_stdin, _process_on_stdin_close);

    if (!self->flag_stdout_exit)
    {
        uv_close((uv_handle_t*)&self->pip_stdout, _process_on_stdout_close);
    }
    if (!self->flag_stderr_exit)
    {
        uv_close((uv_handle_t*)&self->pip_stderr, _process_on_stderr_close);
    }
}
