#include <stdlib.h>
#include <string.h>
#include "runtime.h"
#include "lua/coroutine.h"
#include "lua/process.h"
#include "lua/screenshot.h"
#include "lua/sleep.h"
#include "utils.h"

/******************************************************************************
* Expose lua api and c api to lua vm
******************************************************************************/

/**
 * @brief Lua API list.
 */
#define AUTO_LUA_API_MAP(xx) \
    xx("coroutine",         atd_lua_coroutine)      \
    xx("process",           atd_lua_process)        \
    xx("screenshot",        atd_lua_screenshot)     \
    xx("sleep",             atd_lua_sleep)

#define EXPAND_MAP_AS_PROXY_FUNCTION(name, func)    \
    static int func##_##proxy(lua_State *L) {\
        AUTO_CHECK_TERM();\
        return func(L);\
    }

AUTO_LUA_API_MAP(EXPAND_MAP_AS_PROXY_FUNCTION)

#undef EXPAND_MAP_AS_PROXY_FUNCTION

#define EXPAND_MAP_AS_LUA_FUNCTION(name, func) \
    { name, func##_##proxy },

static const luaL_Reg s_funcs[] = {
    AUTO_LUA_API_MAP(EXPAND_MAP_AS_LUA_FUNCTION)
    { NULL, NULL }
};

#undef EXPAND_MAP_AS_LUA_FUNCTION

void auto_init_libs(lua_State *L)
{
    /* Create lua api table */
    luaL_newlib(L, s_funcs);

    /* Register C api */
    lua_pushlightuserdata(L, &api);
    lua_setfield(L, -2, "api");

    /* Set as global variable */
    lua_setglobal(L, "auto");
}

/******************************************************************************
* C API: new_sem
******************************************************************************/
typedef struct atd_sem_impl
{
    atd_sem_t   handle;
    uv_sem_t    sem;
}atd_sem_impl_t;

static void _sem_destroy(struct atd_sem* thiz)
{
    atd_sem_impl_t* impl = container_of(thiz, atd_sem_impl_t, handle);
    uv_sem_destroy(&impl->sem);
    free(impl);
}

static void _sem_wait(struct atd_sem* thiz)
{
    atd_sem_impl_t* impl = container_of(thiz, atd_sem_impl_t, handle);
    uv_sem_wait(&impl->sem);
}

static void _sem_post(struct atd_sem* thiz)
{
    atd_sem_impl_t* impl = container_of(thiz, atd_sem_impl_t, handle);
    uv_sem_post(&impl->sem);
}

static atd_sem_t* api_new_sem(unsigned int value)
{
    atd_sem_impl_t* impl = malloc(sizeof(atd_sem_impl_t));
    impl->handle.destroy = _sem_destroy;
    impl->handle.wait = _sem_wait;
    impl->handle.post = _sem_post;

    uv_sem_init(&impl->sem, value);
    return &impl->handle;
}

/******************************************************************************
* C API: new_thread
******************************************************************************/
typedef struct atd_thread_impl
{
    atd_thread_t    handle;
    uv_thread_t     thread;
} atd_thread_impl_t;

static void _thread_join(struct atd_thread* thiz)
{
    atd_thread_impl_t* impl = container_of(thiz, atd_thread_impl_t, handle);
    uv_thread_join(&impl->thread);
    free(impl);
}

static atd_thread_t* api_new_thread(atd_thread_fn fn, void* arg)
{
    atd_thread_impl_t* impl = malloc(sizeof(atd_thread_impl_t));
    impl->handle.join = _thread_join;
    if (uv_thread_create(&impl->thread, fn, arg) != 0)
    {
        free(impl);
        return NULL;
    }
    return &impl->handle;
}

/******************************************************************************
* C API: new_async
******************************************************************************/

typedef struct atd_sync_impl
{
    atd_sync_t      handle;
    uv_async_t      async;
    atd_async_fn    fn;
    void*           arg;
} atd_sync_impl_t;

static void _async_on_close(uv_handle_t* handle)
{
    atd_sync_impl_t* impl = container_of((uv_async_t*)handle, atd_sync_impl_t, async);
    free(impl);
}

static void _async_on_active(uv_async_t* handle)
{
    atd_sync_impl_t* impl = container_of(handle, atd_sync_impl_t, async);
    impl->fn(impl->arg);
}

static void _async_destroy(struct atd_sync* thiz)
{
    atd_sync_impl_t* impl = container_of(thiz, atd_sync_impl_t, handle);
    uv_close((uv_handle_t*)&impl->async, _async_on_close);
}

static void _async_send(struct atd_sync* thiz)
{
    atd_sync_impl_t* impl = container_of(thiz, atd_sync_impl_t, handle);
    uv_async_send(&impl->async);
}

static atd_sync_t* api_new_async(atd_async_fn fn, void* arg)
{
    atd_sync_impl_t* impl = malloc(sizeof(atd_sync_impl_t));
    impl->handle.destroy = _async_destroy;
    impl->handle.send = _async_send;
    impl->fn = fn;
    impl->arg = arg;
    uv_async_init(&g_rt->loop, &impl->async, _async_on_active);

    return &impl->handle;
}

/******************************************************************************
* C API: new_timer
******************************************************************************/

typedef struct atd_timer_impl
{
    atd_timer_t     handle;
    uv_timer_t      timer;
    atd_timer_fn    fn;
    void*           arg;
} atd_timer_impl_t;

static void _on_timer_close(uv_handle_t* handle)
{
    atd_timer_impl_t* impl = container_of((uv_timer_t*)handle, atd_timer_impl_t, timer);
    free(impl);
}

static void _timer_on_active(uv_timer_t* handle)
{
    atd_timer_impl_t* impl = container_of(handle, atd_timer_impl_t, timer);
    impl->fn(impl->arg);
}

static void _timer_destroy(struct atd_timer* thiz)
{
    atd_timer_impl_t* impl = container_of(thiz, atd_timer_impl_t, handle);
    uv_close((uv_handle_t*)&impl->timer, _on_timer_close);
}

static void _timer_start(struct atd_timer* thiz, uint64_t timeout,
    uint64_t repeat, atd_timer_fn fn, void* arg)
{
    atd_timer_impl_t* impl = container_of(thiz, atd_timer_impl_t, handle);
    impl->fn = fn;
    impl->arg = arg;
    uv_timer_start(&impl->timer, _timer_on_active, timeout, repeat);
}

static void _timer_stop(struct atd_timer* thiz)
{
    atd_timer_impl_t* impl = container_of(thiz, atd_timer_impl_t, handle);
    uv_timer_stop(&impl->timer);
}

static atd_timer_t* api_new_timer(void)
{
    atd_timer_impl_t* impl = malloc(sizeof(atd_timer_impl_t));

    impl->handle.destroy = _timer_destroy;
    impl->handle.start = _timer_start;
    impl->handle.stop = _timer_stop;
    uv_timer_init(&g_rt->loop, &impl->timer);

    return &impl->handle;
}

/******************************************************************************
* C API: new_process
******************************************************************************/

typedef struct atd_process_impl
{
    atd_process_t           handle;
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
} atd_process_impl_t;

typedef struct atd_process_write
{
    uv_write_t              req;
    atd_process_impl_t*     impl;

    void*                   data;
    size_t                  size;

    atd_process_stdio_fn    cb;
    void*                   arg;
} atd_process_write_t;

static void _process_on_close(atd_process_impl_t* impl)
{
    if (!impl->flag_process_exit || !impl->flag_stdin_exit || !impl->flag_stdout_exit || !impl->flag_stderr_exit)
    {
        return;
    }

    free(impl);
}

static void _process_on_process_close(uv_handle_t* handle)
{
    atd_process_impl_t* impl = container_of((uv_process_t*)handle, atd_process_impl_t, process);
    impl->flag_process_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdin_close(uv_handle_t* handle)
{
    atd_process_impl_t* impl = container_of((uv_pipe_t*)handle, atd_process_impl_t, pip_stdin);
    impl->flag_stdin_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stdout_close(uv_handle_t* handle)
{
    atd_process_impl_t* impl = container_of((uv_pipe_t*)handle, atd_process_impl_t, pip_stdout);
    impl->flag_stdout_exit = 1;
    _process_on_close(impl);
}

static void _process_on_stderr_close(uv_handle_t* handle)
{
    atd_process_impl_t* impl = container_of((uv_pipe_t*)handle, atd_process_impl_t, pip_stderr);
    impl->flag_stderr_exit = 1;
    _process_on_close(impl);
}

static void _process_kill(struct atd_process* thiz, int signum)
{
    atd_process_impl_t* impl = container_of(thiz, atd_process_impl_t, handle);

    if (impl->spawn_ret == 0)
    {
        uv_process_kill(&impl->process, signum);
    }

    uv_close((uv_handle_t*)&impl->process, _process_on_process_close);
    uv_close((uv_handle_t*)&impl->pip_stdin, _process_on_stdin_close);

    if (!impl->flag_stdout_exit)
    {
        uv_close((uv_handle_t*)&impl->pip_stdout, _process_on_stdout_close);
    }
    if (!impl->flag_stderr_exit)
    {
        uv_close((uv_handle_t*)&impl->pip_stderr, _process_on_stderr_close);
    }
}

static void _process_write_done(uv_write_t *req, int status)
{
    atd_process_write_t* p_req = container_of(req, atd_process_write_t, req);

    p_req->cb(&p_req->impl->handle, p_req->data, p_req->size, status, p_req->arg);
    free(p_req);
}

static int _process_send_to_stdin(struct atd_process* thiz, void* data,
    size_t size, atd_process_stdio_fn cb, void* arg)
{
    atd_process_impl_t* impl = container_of(thiz, atd_process_impl_t, handle);

    atd_process_write_t* p_req = malloc(sizeof(atd_process_write_t));
    p_req->impl = impl;
    p_req->cb = cb;
    p_req->arg = arg;
    p_req->size = size;
    p_req->data = data;

    uv_buf_t buf = uv_buf_init(p_req->data, p_req->size);
    return uv_write(&p_req->req, (uv_stream_t*)&impl->pip_stdin, &buf, 1,
        _process_write_done);
}

static void _process_on_exit(uv_process_t* process, int64_t exit_status, int term_signal)
{
    atd_process_impl_t* impl = container_of(process, atd_process_impl_t, process);
    impl->exit_status = exit_status;
    impl->term_signal = term_signal;
}

static void _process_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    buf->len = suggested_size;
    buf->base = malloc(suggested_size);
}

static void _process_on_stdout(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_impl_t* impl = container_of((uv_pipe_t*)stream, atd_process_impl_t, pip_stdout);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stdout_fn(&impl->handle, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static void _process_on_stderr(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    atd_process_impl_t* impl = container_of((uv_pipe_t*)stream, atd_process_impl_t, pip_stderr);

    /* Stop read if error */
    if (nread < 0)
    {
        uv_read_stop(stream);
    }

    if (buf->base != NULL)
    {
        impl->stderr_fn(&impl->handle, buf->base, nread, nread, impl->arg);
        free(buf->base);
    }
}

static atd_process_t* api_new_process(atd_process_cfg_t* cfg)
{
    atd_process_impl_t* impl = malloc(sizeof(atd_process_impl_t));
    memset(impl, 0, sizeof(*impl));

    impl->handle.kill = _process_kill;
    impl->handle.send_to_stdin = _process_send_to_stdin;
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
        impl->handle.kill(&impl->handle, SIGKILL);
        return NULL;
    }

    uv_read_start((uv_stream_t*)&impl->pip_stdout, _process_alloc_cb, _process_on_stdout);
    uv_read_start((uv_stream_t*)&impl->pip_stderr, _process_alloc_cb, _process_on_stderr);

    return &impl->handle;
}

/******************************************************************************
* C API: register_coroutine
******************************************************************************/

static void _coroutine_set_schedule_state(struct atd_coroutine* thiz, int state)
{
    atd_coroutine_impl_t* impl = container_of(thiz, atd_coroutine_impl_t, base);

    /* In busy_queue */
    if (impl->base.status == LUA_TNONE)
    {
        if (state == LUA_TNONE)
        {/* Do nothing if new state is also BUSY */
            return;
        }

        ev_list_erase(&g_rt->schedule.busy_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.wait_queue, &impl->q_node);
        return;
    }

    /* We are in wait_queue, cannot operate on dead coroutine */
    if (impl->base.status != LUA_YIELD)
    {
        abort();
    }

    /* move to busy_queue */
    if (state == LUA_TNONE)
    {
        ev_list_erase(&g_rt->schedule.wait_queue, &impl->q_node);
        impl->base.status = state;
        ev_list_push_back(&g_rt->schedule.busy_queue, &impl->q_node);
    }

    /* thr is dead, keep in wait_queue */
}

static atd_coroutine_hook_t* _coroutine_hook(struct atd_coroutine* thiz, atd_coroutine_hook_fn fn, void* arg)
{
    atd_coroutine_impl_t* impl = container_of(thiz, atd_coroutine_impl_t, base);

    atd_coroutine_hook_t* token = malloc(sizeof(atd_coroutine_hook_t));
    token->fn = fn;
    token->arg = arg;
    token->impl = impl;
    ev_list_push_back(&impl->hook.queue, &token->node);

    return token;
}

static void _coroutine_unhook(struct atd_coroutine* thiz, atd_coroutine_hook_t* token)
{
    atd_coroutine_impl_t* impl = container_of(thiz, atd_coroutine_impl_t, base);

    if (impl->hook.it != NULL)
    {
        atd_coroutine_hook_t* next_hook = container_of(impl->hook.it, atd_coroutine_hook_t, node);

        /* If this is next hook, move iterator to next node. */
        if (next_hook == token)
        {
            impl->hook.it = ev_list_next(impl->hook.it);
        }
    }

    ev_list_erase(&impl->hook.queue, &token->node);
    token->impl = NULL;

    free(token);
}

static atd_coroutine_t* api_register_coroutine(lua_State* L)
{
    atd_coroutine_impl_t* thr = malloc(sizeof(atd_coroutine_impl_t));

    memset(thr, 0, sizeof(*thr));
    thr->base.L = L;
    thr->base.status = LUA_TNONE;
    ev_list_init(&thr->hook.queue);

    thr->base.hook = _coroutine_hook;
    thr->base.unhook = _coroutine_unhook;
    thr->base.set_schedule_state = _coroutine_set_schedule_state;

    /* Save to schedule table to check duplicate */
    if (ev_map_insert(&g_rt->schedule.all_table, &thr->t_node) != NULL)
    {
        free(thr);
        return NULL;
    }

    /* Get reference */
    lua_pushthread(L);
    thr->data.ref_key = luaL_ref(L, LUA_REGISTRYINDEX);

    /* Save to busy_queue */
    ev_list_push_back(&g_rt->schedule.busy_queue, &thr->q_node);

    return &thr->base;
}

/******************************************************************************
* C API: find_coroutine
******************************************************************************/

static atd_coroutine_t* api_find_coroutine(lua_State* L)
{
    atd_coroutine_impl_t tmp;
    tmp.base.L = L;

    ev_map_node_t* it = ev_map_find(&g_rt->schedule.all_table, &tmp.t_node);
    if (it == NULL)
    {
        return NULL;
    }

    atd_coroutine_impl_t* impl = container_of(it, atd_coroutine_impl_t, t_node);
    return &impl->base;
}

atd_api_t api = {
    uv_hrtime,              /* .hrtime */
    api_new_sem,            /* .new_sem */
    api_new_thread,         /* .new_thread */
    api_new_async,          /* .new_async */
    api_new_timer,          /* .new_timer */
    api_new_process,        /* .new_process */
    api_register_coroutine, /* .register_coroutine */
    api_find_coroutine,     /* .find_coroutine */
};
