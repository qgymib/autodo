#include <string.h>
#include <stdlib.h>
#include <llhttp.h>
#include <uv.h>
#include "lua/regex.h"
#include "http_server.h"
#include "runtime.h"
#include "utils.h"

typedef struct http_server_impl
{
    auto_runtime_t*     rt;                 /**< Runtime handle. */
    auto_coroutine_t*   host_co;            /**< Hosting coroutine. */

    uv_tcp_t            listen_sock;        /**< Listening socket. */
    auto_map_t          route_table;        /**< #http_server_route_t. */

    auto_list_t         client_queue;       /**< #http_server_connection_t. Client connections. */

    struct
    {
        char*           listen_url;         /**< Listen URL. e.g. http://127.0.0.1:5000 */
        char*           listen_protocol;    /**< Listen protocol. e.g. http */
        char*           listen_address;     /**< Listen address. e.g. 127.0.0.1 */
        int             listen_port;        /**< Listen port. e.g. 5000 */
    } config;
} http_server_impl_t;

typedef struct http_server_connection
{
    auto_list_node_t    node;               /**< List node. */

    lua_State*          co;                 /**< Coroutine for storage. */
    int                 co_ref;             /**< Reference to coroutine. */

    http_server_impl_t* belong;             /**< Belong server. */
    uv_tcp_t            client_sock;        /**< Client socket. */

    llhttp_t            parser;             /**< Http parser */
    llhttp_settings_t   parser_setting;     /**< Http parser settings */
} http_server_connection_t;

typedef struct http_server_route
{
    auto_map_node_t     node;

    char*               path;               /**< Route path. */
    int                 ref_cb;             /**< Callback reference. */
} http_server_route_t;

typedef struct lua_http_server
{
    http_server_impl_t* impl;               /**< Real http server. */
} lua_http_server_t;

static void _http_server_on_listen_close(uv_handle_t* handle)
{
    http_server_impl_t* impl = container_of((uv_tcp_t*)handle, http_server_impl_t, listen_sock);
    free(impl);
}

static void _http_server_cleanup_config(http_server_impl_t* impl)
{
    if (impl->config.listen_url != NULL)
    {
        free(impl->config.listen_url);
        impl->config.listen_url = NULL;
    }
    if (impl->config.listen_protocol != NULL)
    {
        free(impl->config.listen_protocol);
        impl->config.listen_protocol = NULL;
    }
    if (impl->config.listen_address != NULL)
    {
        free(impl->config.listen_address);
        impl->config.listen_address = NULL;
    }
}

static void _http_server_cleanup_route(lua_State* L, http_server_impl_t* impl)
{
    auto_map_node_t* it = ev_map_begin(&impl->route_table);
    while (it != NULL)
    {
        http_server_route_t* route = container_of(it, http_server_route_t, node);
        it = ev_map_next(it);

        ev_map_erase(&impl->route_table, &route->node);
        luaL_unref(L, LUA_REGISTRYINDEX, route->ref_cb);
        free(route->path);
        free(route);
    }
}

static void _http_server_on_client_close(uv_handle_t* handle)
{
    http_server_connection_t* conn = container_of((uv_tcp_t*)handle, http_server_connection_t, client_sock);
    free(conn);
}

static void _http_server_close_connection(lua_State* L, http_server_impl_t* impl, http_server_connection_t* conn)
{
    ev_list_erase(&impl->client_queue, &conn->node);

    luaL_unref(L, LUA_REGISTRYINDEX, conn->co_ref);
    conn->co = NULL;

    uv_close((uv_handle_t*)&conn->client_sock, _http_server_on_client_close);
}

static void _http_server_cleanup_clients(lua_State* L, http_server_impl_t* impl)
{
    auto_list_node_t* it;
    while ((it = ev_list_begin(&impl->client_queue)) != NULL)
    {
        http_server_connection_t* conn = container_of(it, http_server_connection_t, node);
        _http_server_close_connection(L, impl, conn);
    }
}

static void _http_server_destroy_impl(lua_State* L, http_server_impl_t* impl)
{
    uv_close((uv_handle_t*)&impl->listen_sock, _http_server_on_listen_close);

    _http_server_cleanup_clients(L, impl);
    _http_server_cleanup_route(L, impl);
    _http_server_cleanup_config(impl);
}

static int _http_server_gc(lua_State* L)
{
    lua_http_server_t* self = lua_touserdata(L, 1);

    if (self->impl != NULL)
    {
        _http_server_destroy_impl(L, self->impl);
        self->impl = NULL;
    }

    return 0;
}

static int _http_server_close(lua_State* L)
{
    lua_http_server_t* self = lua_touserdata(L, 1);

    self->impl->host_co = NULL;

    return 0;
}

static int _http_server_check_listen_url(lua_State* L, http_server_impl_t* impl)
{
    if (strcmp(impl->config.listen_protocol, "http") != 0)
    {
        return api.lua->A_error(L, "unknown protocol `%s`", impl->config.listen_protocol);
    }
    return 0;
}

static int _http_server_parse_listen_url(lua_State* L, http_server_impl_t* impl)
{
    int sp = lua_gettop(L);

    lua_pushcfunction(L, auto_lua_regex);
    lua_pushstring(L, "(\\w+)://([\\d.:[a-f][A-F]]+)((?::)(\\d+))?");
    lua_call(L, 1, 1);
    lua_pushstring(L, impl->config.listen_url);
    lua_call(L, 1, 1);

    lua_rawgeti(L, -1, 1);
    lua_rawgeti(L, -1, 3);
    impl->config.listen_protocol = auto_strdup(lua_tostring(L, -1));
    lua_pop(L, 2);

    lua_rawgeti(L, -1, 2);
    lua_rawgeti(L, -1, 3);
    impl->config.listen_address = auto_strdup(lua_tostring(L, -1));
    lua_pop(L, 2);

    if (lua_rawgeti(L, -1, 3) == LUA_TNIL)
    {
        impl->config.listen_port = 5000;
        lua_pushnil(L);
    }
    else
    {
        lua_rawgeti(L, -1, 3);
        if (sscanf(lua_tostring(L, -1), "%d", &impl->config.listen_port) != 1)
        {
            return api.lua->A_error(L, "unknown port %s", lua_tostring(L, -1));
        }
    }
    lua_pop(L, 2);

    lua_settop(L, sp);
    return _http_server_check_listen_url(L, impl);
}

static void _http_server_config(lua_State* L, http_server_impl_t* impl, int idx)
{
    if (lua_getfield(L, idx, "listen_url") == LUA_TSTRING)
    {
        impl->config.listen_url = auto_strdup(lua_tostring(L, -1));
    }
    else
    {
        impl->config.listen_url = auto_strdup("http://127.0.0.1:5000");
    }
    lua_pop(L, 1);
    _http_server_parse_listen_url(L, impl);
}

static int _http_server_is_upv6(const char* addr)
{
    return strstr(addr, ":") != NULL;
}

static void _http_server_on_connection_close(uv_handle_t* handle)
{
    http_server_connection_t* conn = container_of((uv_tcp_t*)handle, http_server_connection_t, client_sock);
    free(conn);
}

static void _http_server_on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    *buf = uv_buf_init(malloc(suggested_size), suggested_size);
}

static void _http_server_on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    http_server_connection_t* conn = container_of((uv_tcp_t*)stream, http_server_connection_t, client_sock);
    http_server_impl_t* impl = conn->belong;

    int ret = llhttp_execute(&conn->parser, buf->base, nread);

    /* Cleanup */
    free(buf->base);

    if (ret != HPE_OK)
    {
        _http_server_close_connection(conn->co, impl, conn);
    }
}

static int _http_server_on_llhttp_begin(llhttp_t* parser)
{
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);

    lua_settop(conn->co, 0);
    lua_newtable(conn->co); // SP:1

    return 0;
}

static int _http_server_on_llhttp_complete(llhttp_t* parser)
{
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);




    return 0;
}

static int _http_server_on_llhttp_header_field(llhttp_t* parser, const char *at, size_t length)
{
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);
    lua_pushlstring(conn->co, at, length);  // SP:2
    return 0;
}

static int _http_server_on_llhttp_header_value(llhttp_t* parser, const char* at, size_t length)
{
    /* The following fields use the newest value.  */
    static const char* s_discarded_fields[] = {
        "age", "authorization", "content-length", "content-type", "etag",
        "expires", "from", "host", "if-modified-since", "if-unmodified-since",
        "last-modified", "location", "max-forwards", "proxy-authorization",
        "referer", "retry-after", "server", "user-agent",
    };
    /* The following fields is always an array. */
    static const char* s_array_fields[] = {
        "set-cookie",
    };
    /* The following fields are joined together with ';' */
    static const char* s_semicolon_fields[] = {
        "cookie",
    };

    size_t i;
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);
    const char* field_name = lua_tostring(conn->co, -1);

    for (i = 0; i < ARRAY_SIZE(s_discarded_fields); i++)
    {
        if (strcmp(field_name, s_discarded_fields[i]) == 0)
        {
            lua_pushlstring(conn->co, at, length);  // SP:3
            lua_rawset(conn->co, 1);    // SP:1
            return 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(s_array_fields); i++)
    {
        if (strcmp(field_name, s_array_fields[i]) == 0)
        {
            if (lua_gettable(conn->co, 1) != LUA_TTABLE)   // SP:3
            {
                lua_pop(conn->co, 1);   // SP:2

                lua_newtable(conn->co); // SP:3
                lua_pushlstring(conn->co, at, length);  // SP:4
                lua_rawseti(conn->co, 3, 1);    // SP:3

                lua_rawset(conn->co, 1);    // SP:1
            }
            else
            {
                lua_pushlstring(conn->co, at, length);  // SP:4
                lua_rawseti(conn->co, 3, luaL_len(conn->co, 3) + 1);    // SP:3
                lua_pop(conn->co, 2);   // SP:1
            }

            return 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(s_semicolon_fields); i++)
    {
        if (strcmp(field_name, s_semicolon_fields[i]) == 0)
        {
            if (lua_gettable(conn->co, 1) != LUA_TSTRING)    // SP:3
            {
                lua_pop(conn->co, 1);   // SP:2
                lua_pushlstring(conn->co, at, length);  // SP:3
                lua_rawset(conn->co, 1);    // SP:1
            }
            else
            {
                lua_pushlstring(conn->co, ";", 1);  // SP:4
                lua_pushlstring(conn->co, at, length);  // SP:5
                lua_concat(conn->co, 3);    // SP:3
                lua_rawset(conn->co, 1);    // SP:1
            }

            return 0;
        }
    }

    /* For all other headers, the values are joined together with ','. */
    if (lua_gettable(conn->co, 1) != LUA_TSTRING)   // SP:3
    {
        lua_pop(conn->co, 1);   // SP:2
        lua_pushlstring(conn->co, at, length);  // SP:3
        lua_rawset(conn->co, 1);    // SP:1
    }
    else
    {
        lua_pushlstring(conn->co, ",", 1);  // SP:4
        lua_pushlstring(conn->co, at, length);  // SP:5
        lua_concat(conn->co, 3);    // SP:3
        lua_rawset(conn->co, 1);    // SP:1
    }

    return 0;
}

static int _http_server_on_llhttp_url(llhttp_t* parser, const char* at, size_t length)
{
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);

    lua_pushlstring(conn->co, at, length);
    lua_setfield(conn->co, -2, "url");
    return 0;
}

static int _http_server_on_llhttp_status(llhttp_t* parser, const char* at, size_t length)
{
    (void)length;
    http_server_connection_t* conn = container_of(parser, http_server_connection_t, parser);

    int status = 0;
    if (sscanf(at, "%d", &status) != 1)
    {
        return 0;
    }

    lua_pushinteger(conn->co, status);
    lua_setfield(conn->co, 1, "status");

    return 0;
}

static void _http_server_on_listen(uv_stream_t* server, int status)
{
    int ret;
    http_server_impl_t* impl = container_of((uv_tcp_t*)server, http_server_impl_t, listen_sock);

    if (status != 0)
    {
        return;
    }

    http_server_connection_t* conn = malloc(sizeof(http_server_connection_t));
    conn->belong = impl;
    conn->co = lua_newthread(impl->host_co->L);
    conn->co_ref = luaL_ref(impl->host_co->L, LUA_REGISTRYINDEX);

    ret = uv_tcp_init(&impl->rt->loop, &conn->client_sock);
    if (ret != 0)
    {
        free(conn);
        return;
    }

    ret = uv_accept(server, (uv_stream_t*)&conn->client_sock);
    if (ret != 0)
    {
        goto error;
    }

    ret = uv_read_start((uv_stream_t*)&conn->client_sock, _http_server_on_alloc, _http_server_on_read);
    if (ret != 0)
    {
        goto error;
    }

    llhttp_settings_init(&conn->parser_setting);
    conn->parser_setting.on_url = _http_server_on_llhttp_url;
    conn->parser_setting.on_status = _http_server_on_llhttp_status;
    conn->parser_setting.on_header_field =_http_server_on_llhttp_header_field;
    conn->parser_setting.on_header_value = _http_server_on_llhttp_header_value;
    conn->parser_setting.on_message_begin = _http_server_on_llhttp_begin;
    conn->parser_setting.on_message_complete = _http_server_on_llhttp_complete;
    llhttp_init(&conn->parser, HTTP_BOTH, &conn->parser_setting);

    ev_list_push_back(&impl->client_queue, &conn->node);

    return;

error:
    uv_close((uv_handle_t*)&conn->client_sock, _http_server_on_connection_close);
}

static int _http_server_on_resume(lua_State* L, int status, lua_KContext ctx)
{
    http_server_impl_t* impl = (http_server_impl_t*)ctx;

    if (impl->host_co == NULL)
    {
        return 0;
    }

    api.coroutine->set_state(impl->host_co, AUTO_COROUTINE_WAIT);
    return lua_yieldk(L, 0, (lua_KContext)impl, _http_server_on_resume);
}

static int _http_server_impl_run(lua_State* L, http_server_impl_t* impl)
{
    int ret;

    /* Bind to address. */
    struct sockaddr_storage listen_addr;
    if (_http_server_is_upv6(impl->config.listen_address))
    {
        ret = uv_ip6_addr(impl->config.listen_address, impl->config.listen_port, (struct sockaddr_in6*)&listen_addr);
    }
    else
    {
        ret = uv_ip4_addr(impl->config.listen_address, impl->config.listen_port, (struct sockaddr_in*)&listen_addr);
    }
    if (ret != 0)
    {
        return api.lua->A_error(L, "%s", uv_strerror(ret));
    }
    ret = uv_tcp_bind(&impl->listen_sock, (struct sockaddr*)&listen_addr, 0);
    if (ret != 0)
    {
        return api.lua->A_error(L, "%s", uv_strerror(ret));
    }

    ret = uv_listen((uv_stream_t*)&impl->listen_sock, 1024, _http_server_on_listen);
    if (ret != 0)
    {
        return api.lua->A_error(L, "%s", uv_strerror(ret));
    }

    return _http_server_on_resume(L, LUA_OK, (lua_KContext)impl);
}

static int _http_server_run(lua_State* L)
{
    lua_http_server_t* self = lua_touserdata(L, 1);

    self->impl->host_co = api.coroutine->find(L);

    return _http_server_impl_run(L, self->impl);
}

static int _http_server_route(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_http_server_t* self = lua_touserdata(L, 1);

    const char* path = luaL_checkstring(L, 2);

    http_server_route_t* route = malloc(sizeof(http_server_route_t));
    route->path = auto_strdup(path);

    lua_pushvalue(L, 3);
    route->ref_cb = luaL_ref(L, LUA_REGISTRYINDEX);

    if (ev_map_insert(&self->impl->route_table, &route->node) != NULL)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, route->ref_cb);
        free(route->path);
        free(route);
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static void _http_server_setmetatable(lua_State* L)
{
    static const luaL_Reg s_http_server_meta[] = {
            { "__gc",       _http_server_gc },
            { NULL,         NULL },
    };
    static const luaL_Reg s_http_server_method[] = {
            { "close",      _http_server_close },
            { "route",      _http_server_route },
            { "run",        _http_server_run },
            { NULL,         NULL },
    };
    if (luaL_newmetatable(L, "__auto_process") != 0)
    {
        luaL_setfuncs(L, s_http_server_meta, 0);
        luaL_newlib(L, s_http_server_method);

        lua_setfield(L, -2, "__index");
    }
    lua_setmetatable(L, -2);
}

static int _http_server_on_cmp_route(const auto_map_node_t* key1,
    const auto_map_node_t* key2, void* arg)
{
    (void)arg;
    http_server_route_t* r1 = container_of(key1, http_server_route_t, node);
    http_server_route_t* r2 = container_of(key2, http_server_route_t, node);
    return strcmp(r1->path, r2->path);
}

static int _http_server_init(lua_State* L, http_server_impl_t* impl)
{
    int ret;

    ev_list_init(&impl->client_queue);
    ev_map_init(&impl->route_table, _http_server_on_cmp_route, NULL);

    /* Initialize listen socket. */
    ret = uv_tcp_init(&impl->rt->loop, &impl->listen_sock);
    if (ret != 0)
    {
        return api.lua->A_error(L, "%s", uv_strerror(ret));
    }

    return 0;
}

int auto_lua_http_server(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_http_server_t* self = lua_newuserdata(L, sizeof(lua_http_server_t));
    memset(self, 0, sizeof(*self));
    _http_server_setmetatable(L);

    self->impl = malloc(sizeof(http_server_impl_t));
    memset(self->impl, 0, sizeof(*self->impl));

    self->impl->rt = auto_get_runtime(L);
    _http_server_config(L, self->impl, 1);
    _http_server_init(L, self->impl);

    return 1;
}
