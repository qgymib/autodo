#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <llhttp.h>
#include "http_message.h"
#include "utils/map.h"
#include "utils.h"

#define EXPAND_FIELD_AS_ARRAY(name) { name, sizeof(name) },

#define HTTP_MESSAGE_DISCARDED_FIELD(xx)    \
    xx("age")                   \
    xx("authorization")         \
    xx("content-length")        \
    xx("content-type")          \
    xx("etag")                  \
    xx("expires")               \
    xx("from")                  \
    xx("host")                  \
    xx("if-modified-since")     \
    xx("if-unmodified-since")   \
    xx("last-modified")         \
    xx("location")              \
    xx("max-forwards")          \
    xx("proxy-authorization")   \
    xx("referer")               \
    xx("retry-after")           \
    xx("server")                \
    xx("user-agent")

#define HTTP_MESSAGE_SEMICOLON_FIELD(xx)    \
    xx("cookie")

#define HTTP_MESSAGE_DUP(p, FIELD, data, length) \
    do {\
        http_message_parser_t* _self = container_of(p, http_message_parser_t, parser);\
        http_message_impl_t* _msg = _self->on_going;\
        if (_msg->base.FIELD != NULL) {\
            free(_msg->base.FIELD);\
        }\
        _msg->base.FIELD = malloc(length + 1);\
        _msg->base.FIELD##_sz = length;\
        memcpy(_msg->base.FIELD, data, length);\
        _msg->base.FIELD[length] = '\0';\
    } while (0);\
    return 0

#define HTTP_MESSAGE_PRINT_TO_LUA(L, idx, msg, FIELD)   \
    do {\
        if (msg->base.FIELD != NULL) {\
            lua_pushlstring(L, msg->base.FIELD, msg->base.FIELD##_sz);\
            lua_setfield(L, idx, #FIELD);\
        }\
    } while (0)

typedef struct http_message_header
{
    auto_map_node_t         node;

    size_t                  value_sz;   /**< Length of field value, not including NULL terminator. */
    char*                   value;      /**< Field value. Don't free it, free value_ar. */
    char**                  value_ar;   /**< Field value array. [0] = value, [1] = NULL. */

    size_t                  name_sz;    /**< Length of field name, not including NULL terminator. */
    char*                   name;       /**< Field name. No need to free. */
} http_message_header_t;

typedef struct http_message_impl
{
    http_message_t          base;

    /* `set-cookie` is always an array. */
    struct
    {
        size_t              capacity;   /**< cookie array capacity, NULL is not included. */
        size_t              size;       /**< cookie array size, NULL is not included. */
        char**              contents;   /**< cookie array, NULL is always appended. */
    } set_cookie;

    auto_map_t              header_table;
    http_message_header_t*  last_header;
} http_message_impl_t;

struct http_message_parser
{
    llhttp_t                    parser;             /**< Http parser */
    llhttp_settings_t           setting;            /**< Http parser settings */

    http_message_impl_t*        on_going;
    int                         is_complete;
};

typedef struct http_message_field_matcher
{
    const char*             name;
    size_t                  size;
} http_message_field_matcher_t;

static http_message_field_matcher_t s_http_message_discarded_fields[] = {
    HTTP_MESSAGE_DISCARDED_FIELD(EXPAND_FIELD_AS_ARRAY)
};

static http_message_field_matcher_t s_http_message_semicolon_fields[] = {
    HTTP_MESSAGE_SEMICOLON_FIELD(EXPAND_FIELD_AS_ARRAY)
};

static int _http_message_on_cmp_header(const auto_map_node_t* key1,
    const auto_map_node_t* key2, void* arg)
{
    (void)arg;

    http_message_header_t* h1 = container_of(key1, http_message_header_t, node);
    http_message_header_t* h2 = container_of(key2, http_message_header_t, node);
    return strcasecmp(h1->name, h2->name);
}

static int _on_parser_begin(llhttp_t* parser)
{
    http_message_parser_t* self = container_of(parser, http_message_parser_t, parser);

    if (self->on_going != NULL)
    {
        http_message_destroy(&self->on_going->base);
        self->on_going = NULL;
    }
    self->is_complete = 0;

    if ((self->on_going = malloc(sizeof(http_message_t))) == NULL)
    {
        return ENOMEM;
    }
    memset(self->on_going, 0, sizeof(*self->on_going));

    http_message_impl_t* msg = self->on_going;

    ev_map_init(&msg->header_table, _http_message_on_cmp_header, NULL);
    msg->set_cookie.capacity = 1;
    if ((msg->set_cookie.contents = malloc(sizeof(char*) * (msg->set_cookie.capacity + 1))) == NULL)
    {
        return ENOMEM;
    }
    msg->set_cookie.contents[0] = NULL;

    return 0;
}

static int _http_message_on_complete(llhttp_t* parser)
{
    http_message_parser_t* self = container_of(parser, http_message_parser_t, parser);

    self->is_complete = 1;
    return 0;
}

static int _http_message_on_url(llhttp_t* parser, const char* at, size_t length)
{
    HTTP_MESSAGE_DUP(parser, url, at, length);
}

static int _http_message_on_status(llhttp_t* parser, const char* at, size_t length)
{
    HTTP_MESSAGE_DUP(parser, status, at, length);
}

static int _http_message_on_method(llhttp_t* parser, const char* at, size_t length)
{
    HTTP_MESSAGE_DUP(parser, method, at, length);
}

static int _http_message_on_version(llhttp_t* parser, const char* at, size_t length)
{
    HTTP_MESSAGE_DUP(parser, version, at, length);
}

static int _http_message_on_header_field(llhttp_t* parser, const char* at, size_t length)
{
    http_message_parser_t* self = container_of(parser, http_message_parser_t, parser);
    http_message_impl_t* msg = self->on_going;

    if (strcmp(at, "set-cookie") == 0)
    {
        msg->last_header = NULL;
        return 0;
    }

    http_message_header_t* header = malloc(sizeof(http_message_header_t) + length + 1);
    header->name = (char*)(header + 1);
    header->name_sz = length;
    memcpy(header->name, at, length);
    header->name[length] = '\0';
    header->value = NULL;
    header->value_ar = NULL;

    auto_map_node_t* orig_node = ev_map_insert(&msg->header_table, &header->node);
    if (orig_node == NULL)
    {
        msg->last_header = header;
    }
    else
    {
        msg->last_header = container_of(orig_node, http_message_header_t, node);
        free(header);
    }

    return 0;
}

static int _http_message_ensure_set_cookie(http_message_impl_t* msg)
{
    if (msg->set_cookie.size < msg->set_cookie.capacity)
    {
        return 0;
    }

    size_t new_capacity = msg->set_cookie.capacity * 2;
    size_t new_size = sizeof(char*) * (new_capacity + 1);
    char** new_contents = realloc(msg->set_cookie.contents, new_size);
    if (new_contents == NULL)
    {
        return -1;
    }

    msg->set_cookie.contents = new_contents;
    msg->set_cookie.capacity = new_capacity;

    return 0;
}

static char _http_message_get_join_code(const char* at, size_t length)
{
    size_t i;
    for (i = 0; i < ARRAY_SIZE(s_http_message_discarded_fields); i++)
    {
        if (length != s_http_message_discarded_fields[i].size)
        {
            continue;
        }

        if (strncasecmp(at, s_http_message_discarded_fields[i].name, length) == 0)
        {
            return 0;
        }
    }

    for (i = 0; i < ARRAY_SIZE(s_http_message_semicolon_fields); i++)
    {
        if (length != s_http_message_semicolon_fields[i].size)
        {
            continue;
        }
        if (strncasecmp(at, s_http_message_semicolon_fields[i].name, length) == 0)
        {
            return ';';
        }
    }

    return ',';
}

static int _http_message_on_header_value(llhttp_t* parser, const char* at, size_t length)
{
    size_t malloc_size;
    http_message_parser_t* self = container_of(parser, http_message_parser_t, parser);
    http_message_impl_t* msg = self->on_going;
    http_message_header_t* header = msg->last_header;

    /* set_cookie */
    if (header == NULL)
    {
        if (_http_message_ensure_set_cookie(msg) != 0)
        {
            return ENOMEM;
        }

        msg->set_cookie.contents[msg->set_cookie.size] = malloc(length + 1);
        if (msg->set_cookie.contents[msg->set_cookie.size] == NULL)
        {
            return ENOMEM;
        }
        memcpy(msg->set_cookie.contents[msg->set_cookie.size], at, length);
        msg->set_cookie.contents[msg->set_cookie.size][length] = '\0';
        msg->set_cookie.size++;
        msg->set_cookie.contents[msg->set_cookie.size] = NULL;

        return 0;
    }

    if (header->value_ar == NULL)
    {
        goto copy_value;
    }

    char join_code = _http_message_get_join_code(at, length);
    if (join_code == 0)
    {
        goto copy_value;
    }

    size_t new_value_size = header->value_sz + 1 + length;
    malloc_size = sizeof(char*) * 2 + new_value_size + 1;

    char** new_value = realloc(header->value_ar, malloc_size);
    if (new_value == NULL)
    {
        return ENOMEM;
    }
    header->value_ar = new_value;
    header->value_ar[0] = (char*)new_value + 2;
    header->value = header->value_ar[0];

    header->value[header->value_sz] = join_code;
    memcpy(header->value + header->value_sz + 1, at, length);
    header->value_sz = new_value_size;
    header->value[header->value_sz] = '\0';

    return 0;

copy_value:
    if (header->value_ar != NULL)
    {
        free(header->value_ar);
        header->value_ar = NULL;
        header->value = NULL;
    }

    malloc_size = sizeof(char*) * 2 + length + 1;
    if ((header->value_ar = malloc(malloc_size)) == NULL)
    {
        return ENOMEM;
    }
    header->value_ar[0] = (char*)header->value_ar + 2;
    header->value_ar[1] = NULL;

    header->value_sz = length;
    header->value = header->value_ar[0];
    memcpy(header->value, at, length);
    header->value[length] = '\0';

    return 0;
}

http_message_parser_t* http_message_parser_create(void)
{
    http_message_parser_t* self = malloc(sizeof(http_message_parser_t));
    self->on_going = NULL;

    llhttp_settings_init(&self->setting);
    self->setting.on_message_begin = _on_parser_begin;
    self->setting.on_url = _http_message_on_url;
    self->setting.on_status = _http_message_on_status;
    self->setting.on_method = _http_message_on_method;
    self->setting.on_version = _http_message_on_version;
    self->setting.on_header_field = _http_message_on_header_field;
    self->setting.on_header_value = _http_message_on_header_value;
    self->setting.on_message_complete = _http_message_on_complete;
    llhttp_init(&self->parser, HTTP_BOTH, &self->setting);

    return self;
}

int http_message_parser_execute(http_message_parser_t* parser, const char* data, size_t len, http_message_t** msg)
{
    int ret = llhttp_execute(&parser->parser, data, len);
    if (ret != 0)
    {
        return -1;
    }

    if (parser->is_complete)
    {
        *msg = &parser->on_going->base;
        parser->on_going = NULL;
    }

    return 0;
}

void http_message_parser_destroy(http_message_parser_t* parser)
{
    if (parser->on_going != NULL)
    {
        http_message_destroy(&parser->on_going->base);
        parser->on_going = NULL;
    }

    free(parser);
}

char** http_message_get_header(http_message_t* msg, const char* name, size_t* length)
{
    http_message_impl_t* msg_impl = container_of(msg, http_message_impl_t, base);

    if (strcasecmp(name, "set-cookie") == 0)
    {
        if (length != NULL)
        {
            *length = msg_impl->set_cookie.size;
        }
        return msg_impl->set_cookie.size != 0 ? msg_impl->set_cookie.contents : NULL;
    }

    http_message_header_t tmp;
    tmp.name = (char*)name;

    auto_map_node_t* it = ev_map_find(&msg_impl->header_table, &tmp.node);
    if (it == NULL)
    {
        if (length != NULL)
        {
            *length = 0;
        }
        return NULL;
    }

    http_message_header_t* field = container_of(it, http_message_header_t, node);

    if (length != NULL)
    {
        *length = 1;
    }
    return field->value_ar;
}

void http_message_destroy(http_message_t* msg)
{
    http_message_impl_t* msg_impl = container_of(msg, http_message_impl_t, base);

    free(msg->url);
    free(msg->status);
    free(msg->method);
    free(msg->version);

    free(msg_impl->set_cookie.contents);

    auto_map_node_t* it = ev_map_begin(&msg_impl->header_table);
    while (it != NULL)
    {
        http_message_header_t* header = container_of(it, http_message_header_t, node);
        it = ev_map_next(it);

        ev_map_erase(&msg_impl->header_table, &header->node);
        free(header->value_ar);
        free(header);
    }
}

int http_message_print(lua_State* L, int idx, http_message_t* msg)
{
    size_t i;
    idx = lua_absindex(L, idx);

    http_message_impl_t* msg_impl = container_of(msg, http_message_impl_t, base);

    HTTP_MESSAGE_PRINT_TO_LUA(L, idx, msg_impl, url);
    HTTP_MESSAGE_PRINT_TO_LUA(L, idx, msg_impl, status);
    HTTP_MESSAGE_PRINT_TO_LUA(L, idx, msg_impl, method);
    HTTP_MESSAGE_PRINT_TO_LUA(L, idx, msg_impl, version);

    if (msg_impl->set_cookie.size != 0)
    {
        lua_newtable(L);
        for (i = 0; i < msg_impl->set_cookie.size; i++)
        {
            lua_pushstring(L, msg_impl->set_cookie.contents[i]);
            lua_seti(L, -2, luaL_len(L, -2) + 1);
        }
        lua_setfield(L, idx, "set-cookie");
    }

    auto_map_node_t* it = ev_map_begin(&msg_impl->header_table);
    for (; it != NULL; it = ev_map_next(it))
    {
        http_message_header_t* header = container_of(it, http_message_header_t, node);
        lua_pushlstring(L, header->value, header->value_sz);
        lua_setfield(L, idx, header->name);
    }

    return 0;
}
