#include <uv.h>
#include <string.h>
#include "async.h"

struct auto_async_s
{
    lua_State*      L;
    int             ref_co;

    auto_notify_t*  notifier;
    auto_list_t     call_queue;
    uv_mutex_t      call_queue_lock;
};

typedef struct async_call_record
{
    auto_list_node_t    node;
    struct
    {
        uv_sem_t        sem;

        auto_async_cb   cb;
        void*           arg;
        int             ret;

        auto_async_t*   belong;
    } data;
} async_call_record_t;

static int _async_lua(lua_State* L)
{
    async_call_record_t* record = lua_touserdata(L, 1);

    record->data.cb(L, record->data.arg);
    return 0;
}

static void _async_do_call(auto_async_t* self);

static int _async_after_call(struct lua_State* L, int status, void* ctx)
{
    (void)L; (void)status;

    async_call_record_t* record = ctx;

    uv_sem_post(&record->data.sem);
    _async_do_call(record->data.belong);

    return 0;
}

static void _async_do_call(auto_async_t* self)
{
    auto_list_node_t* node;

    for(;;)
    {
        uv_mutex_lock(&self->call_queue_lock);
        node = api.list->pop_front(&self->call_queue);
        uv_mutex_unlock(&self->call_queue_lock);

        if (node == NULL)
        {
            break;
        }

        async_call_record_t* record = container_of(node, async_call_record_t, node);

        lua_pushcfunction(self->L, _async_lua);
        lua_pushlightuserdata(self->L, record);

        api.lua->A_callk(self->L, 1, 0, record, _async_after_call);
    }
}

static void _async_on_notify(void* arg)
{
    auto_async_t* self = arg;
    _async_do_call(self);
}

static void _async_cancel_all(auto_async_t* self)
{
    auto_list_node_t* node;
    for (;;)
    {
        uv_mutex_lock(&self->call_queue_lock);
        node = api.list->pop_front(&self->call_queue);
        uv_mutex_unlock(&self->call_queue_lock);

        if (node == NULL)
        {
            break;
        }

        async_call_record_t* record = container_of(node, async_call_record_t, node);
        record->data.ret = 0;

        uv_sem_post(&record->data.sem);
    }
}

static void _async_destroy(auto_async_t* self)
{
    _async_cancel_all(self);

    if (self->ref_co != LUA_NOREF)
    {
        luaL_unref(self->L, LUA_REGISTRYINDEX, self->ref_co);
        self->ref_co = LUA_NOREF;
    }
    self->L = NULL;

    if (self->notifier != NULL)
    {
        api.notify->destroy(self->notifier);
        self->notifier = NULL;
    }

    uv_mutex_destroy(&self->call_queue_lock);
}

static auto_async_t* _async_create(lua_State* L)
{
    auto_async_t* self = malloc(sizeof(auto_async_t));
    memset(self, 0, sizeof(*self));

    api.list->init(&self->call_queue);
    uv_mutex_init(&self->call_queue_lock);

    lua_pushthread(L);
    self->ref_co = luaL_ref(L, LUA_REGISTRYINDEX);
    self->L = L;

    self->notifier = api.notify->create(L, _async_on_notify, self);
    if (self->notifier == NULL)
    {
        _async_destroy(self);
        return NULL;
    }

    return self;
}

static int _async_call_in_lua(auto_async_t* self, auto_async_cb cb, void* arg)
{
    async_call_record_t record;

    record.data.belong = self;
    record.data.ret = 1;
    record.data.cb = cb;
    record.data.arg = arg;
    uv_sem_init(&record.data.sem, 0);

    uv_mutex_lock(&self->call_queue_lock);
    api.list->push_back(&self->call_queue, &record.node);
    uv_mutex_unlock(&self->call_queue_lock);

    uv_sem_wait(&record.data.sem);
    uv_sem_destroy(&record.data.sem);

    return record.data.ret;
}

const auto_api_async_t api_async = {
    _async_create,
    _async_destroy,
    _async_call_in_lua,
    _async_cancel_all,
};
