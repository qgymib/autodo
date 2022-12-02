#include "http_server.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "utils.h"

typedef enum uv_http_action_type
{
    UV_HTTP_ACTION_SEND,
    UV_HTTP_ACTION_SERVE,
} uv_http_action_type_t;

typedef struct uv_http_send_token
{
    uv_write_t                  req;            /**< Write token */
    size_t                      size;           /**< Data size */
    char*                       data;           /**< Data. No need to free. */
} uv_http_send_token_t;

typedef struct uv_http_serve_token
{
    uv_work_t                   req;            /**< Request token. */

    uv_http_str_t               url;            /**< URL. No need to free. */
    uv_http_str_t               root_path;      /**< Root path. No need to free. */
    uv_http_str_t               ssi_pattern;    /**< SSI. No need to free. */
    uv_http_str_t               extra_headers;  /**< Extra headers. No need to free. */
    uv_http_str_t               page404;        /**< Path to 404 page. No need to free. */
    uv_http_str_t               if_none_match;  /**< Value of `If-None-Match`. No need to free. */
    uv_http_str_t               range;          /**< Value of `Range`. No need to free. */
    uv_http_fs_t*               fs;             /**< Filesystem instance. */

    uv_http_str_t               rsp;            /**< Response message. MUST free. */
} uv_http_serve_token_t;

typedef struct uv_http_action
{
    auto_list_node_t            node;

    uv_http_conn_t*             belong;         /**< HTTP connection. */
    uv_http_action_type_t       type;           /**< Action type. */

    union
    {
        uv_http_send_token_t    send;
        uv_http_serve_token_t   serve;
    } as;
} uv_http_action_t;

static uv_http_str_t s_empty_str = UV_HTTP_STR_INIT;

/**
 * @brief Active HTTP connection.
 * @param[in] conn  HTTP connection.
 * @return          UV error code.
 */
static int _uv_http_active_connection(uv_http_conn_t* conn);

static void _uv_http_str_destroy(uv_http_str_t* str)
{
    if (str->ptr != NULL && str->cap != 0)
    {
        free(str->ptr);
    }
    str->ptr = NULL;
    str->len = 0;
    str->cap = 0;
}

/**
 * @brief Ensure \p str have enough capacity for \p size.
 * @param[in] str   String container.
 * @param[in] size  Required size, not including NULL terminator.
 * @return          UV error code.
 */
static int _uv_http_str_ensure_size(uv_http_str_t* str, size_t size)
{
    /* Check if it is a constant string. */
    if (str->ptr != NULL && str->cap == 0)
    {
        abort();
    }

    if (str->cap >= size)
    {
        return 0;
    }

    size_t aligned_size = ALIGN_WITH(size, sizeof(void*));
    size_t double_cap = str->cap << 1;
    size_t new_cap_plus_one = aligned_size > double_cap ? aligned_size : double_cap;

    void* new_ptr = realloc(str->ptr, new_cap_plus_one);
    if (new_ptr == NULL)
    {
        return UV_ENOMEM;
    }

    str->ptr = new_ptr;
    str->cap = new_cap_plus_one - 1;
    return 0;
}

static int _uv_http_str_append(uv_http_str_t* str, const void* at, size_t length)
{
    size_t required_size = str->len + length;
    int ret = _uv_http_str_ensure_size(str, required_size);
    if (ret != 0)
    {
        return ret;
    }

    memcpy(str->ptr + str->len, at, length);
    str->ptr[required_size] = '\0';
    str->len = required_size;

    return 0;
}

static int _uv_http_str_append_c(uv_http_str_t* str, char c)
{
    size_t required_size = str->len + 1;
    int ret = _uv_http_str_ensure_size(str, required_size);
    if (ret != 0)
    {
        return ret;
    }

    str->ptr[str->len] = c;
    str->ptr[required_size] = '\0';

    return 0;
}

static int _uv_http_str_vprintf(uv_http_str_t* str, const char* fmt, va_list ap)
{
    int ret = vsnprintf(NULL, 0, fmt, ap);

    size_t new_size = str->len + ret;
    char* new_ptr = realloc(str->ptr, new_size + 1);
    if (new_ptr == NULL)
    {
        return UV_ENOMEM;
    }
    if (vsnprintf(new_ptr + str->len, ret + 1, fmt, ap) != ret)
    {
        abort();
    }

    str->ptr = new_ptr;
    str->len = new_size;

    return ret;
}

static int _uv_http_str_printf(uv_http_str_t* str, const char* fmt, ...)
{
    int ret;
    va_list ap;
    va_start(ap, fmt);
    {
        ret = _uv_http_str_vprintf(str, fmt, ap);
    }
    va_end(ap);

    return ret;
}

static void _uv_http_default_cb(uv_http_conn_t* conn, int evt, void* data, void* arg)
{
    (void)conn; (void)evt; (void)data; (void)arg;
}

static int _uv_http_listen_parse_url(const char* url, char* ip, int* port)
{
    size_t pos;
    if (strncmp(url, "http://", 7) == 0)
    {
        url += 7;

        int is_ipv6 = 0;
        int is_ipv6_end = 0;
        for (pos = 0; url[pos] != '\0'; pos++)
        {
            switch (url[pos])
            {
            case '[':
                if (pos != 0)
                {
                    return -1;
                }
                is_ipv6 = 1;
                break;

            case ']':
                if (!is_ipv6)
                {
                    return -1;
                }
                memcpy(ip, url + 1, pos - 2);
                is_ipv6_end = 1;
                break;

            case ':':
                if (pos == 0)
                {
                    return -1;
                }
                if (is_ipv6 && !is_ipv6_end)
                {
                    break;
                }
                if (!is_ipv6)
                {
                    memcpy(ip, url, pos);
                }
                if (sscanf(url + pos + 1, "%d", port) != 1)
                {
                    return -1;
                }
                break;

            default:
                break;
            }
        }

        return 0;
    }

    return -1;
}

static int _uv_http_bind_address(uv_http_t* http, const char* url)
{
    int ret;

    char ip[64]; int port;
    if (_uv_http_listen_parse_url(url, ip, &port) != 0)
    {
        return -1;
    }

    struct sockaddr_storage listen_addr;
    if (strstr(ip, ":"))
    {
        if ((ret = uv_ip6_addr(ip, port, (struct sockaddr_in6*)&listen_addr)) != 0)
        {
            return ret;
        }
    }
    else
    {
        if ((ret = uv_ip4_addr(ip, port, (struct sockaddr_in*)&listen_addr)) != 0)
        {
            return ret;
        }
    }

    if ((ret = uv_tcp_bind(&http->listen_sock, (struct sockaddr*)&listen_addr, 0)) != 0)
    {
        return ret;
    }

    return 0;
}

static void _uv_http_on_connection_close(uv_handle_t* handle)
{
    uv_http_conn_t* conn = container_of((uv_tcp_t*)handle, uv_http_conn_t, client_sock);
    free(conn);
}

static void _uv_http_close_connection(uv_http_t* http, uv_http_conn_t* conn, int cb)
{
    ev_list_erase(&http->client_table, &conn->c_node);
    uv_close((uv_handle_t*)&conn->client_sock, _uv_http_on_connection_close);

    if (cb)
    {
        http->cb(conn, UV_HTTP_CLOSE, NULL, http->arg);
    }
}

static void _uv_http_on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    (void)handle;
    *buf = uv_buf_init(malloc(suggested_size), suggested_size);
}

static void _uv_http_on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    int ret;
    uv_http_conn_t* conn = container_of((uv_tcp_t*)stream, uv_http_conn_t, client_sock);
    uv_http_t* http = conn->belong;

    if (nread < 0)
    {
        _uv_http_close_connection(http, conn, 1);
        return;
    }

    ret = llhttp_execute(&conn->parser, buf->base, nread);
    free(buf->base);

    if (ret != 0)
    {
        _uv_http_close_connection(http, conn, 1);
        return;
    }
}

static int _uv_http_on_parser_ensure_headers(uv_http_message_t* msg)
{
    if (msg->header_len < msg->header_cap)
    {
        return 0;
    }

    size_t new_cap = msg->header_cap * 2;
    size_t new_size = sizeof(uv_http_header_t) * new_cap;
    uv_http_header_t* new_header = realloc(msg->headers, new_size);
    if (new_header == NULL)
    {
        return UV_ENOMEM;
    }

    msg->headers = new_header;
    msg->header_cap = new_cap;
    return 0;
}

static int _uv_http_on_parser_begin(llhttp_t* parser)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);

    const size_t default_header_cap = 32;

    if ((conn->on_parsing = malloc(sizeof(uv_http_message_t))) == NULL)
    {
        return UV_ENOMEM;
    }
    memset(conn->on_parsing, 0, sizeof(*conn->on_parsing));

    size_t malloc_size = sizeof(uv_http_header_t) * (default_header_cap + 1);
    if ((conn->on_parsing->headers = malloc(malloc_size)) == NULL)
    {
        free(conn->on_parsing);
        conn->on_parsing = NULL;
        return UV_ENOMEM;
    }
    memset(conn->on_parsing->headers, 0, malloc_size);
    conn->on_parsing->header_cap = default_header_cap;

    return 0;
}

static int _uv_http_on_parser_url(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;
    return _uv_http_str_append(&msg->url, at, length);
}

static int _uv_http_on_parser_status(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;
    return _uv_http_str_append(&msg->status, at, length);
}

static int _uv_http_on_parser_method(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;
    return _uv_http_str_append(&msg->method, at, length);
}

static int _uv_http_on_parser_version(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;
    return _uv_http_str_append(&msg->version, at, length);
}

static int _uv_http_on_parser_header_field(llhttp_t* parser, const char* at, size_t length)
{
    int ret;
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;

    if ((ret = _uv_http_on_parser_ensure_headers(msg)) != 0)
    {
        return ret;
    }

    return _uv_http_str_append(&msg->headers[msg->header_len].name, at, length);
}

static int _uv_http_on_parser_header_value(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;

    return _uv_http_str_append(&msg->headers[msg->header_len].value, at, length);
}

static int _uv_http_on_parser_header_value_complete(llhttp_t* parser)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;

    msg->header_len++;
    return 0;
}

static int _uv_http_on_parser_body(llhttp_t* parser, const char* at, size_t length)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_message_t* msg = conn->on_parsing;
    return _uv_http_str_append(&msg->body, at, length);
}

static void _uv_http_destroy_message(uv_http_message_t* msg)
{
    _uv_http_str_destroy(&msg->url);
    _uv_http_str_destroy(&msg->status);
    _uv_http_str_destroy(&msg->version);
    _uv_http_str_destroy(&msg->body);
    _uv_http_str_destroy(&msg->method);

    size_t i;
    for (i = 0; i < msg->header_len; i++)
    {
        _uv_http_str_destroy(&msg->headers[i].name);
        _uv_http_str_destroy(&msg->headers[i].value);
    }
    free(msg->headers);
    msg->header_len = 0;
    msg->header_cap = 0;
    free(msg);
}

static int _uv_http_on_parser_complete(llhttp_t* parser)
{
    uv_http_conn_t* conn = container_of(parser, uv_http_conn_t, parser);
    uv_http_t* http = conn->belong;

    http->cb(conn, UV_HTTP_MESSAGE, conn->on_parsing, http->arg);

    _uv_http_destroy_message(conn->on_parsing);
    conn->on_parsing = NULL;

    return 0;
}

static void _uv_http_on_listen(uv_stream_t* server, int status)
{
    int ret;
    uv_http_t* http = container_of((uv_tcp_t*)server, uv_http_t, listen_sock);

    if (status != 0)
    {
        return;
    }

    uv_http_conn_t* conn = malloc(sizeof(uv_http_conn_t));
    memset(conn, 0, sizeof(*conn));

    conn->belong = http;
    ev_list_init(&conn->action_queue);

    llhttp_settings_init(&conn->parser_setting);
    conn->parser_setting.on_message_begin = _uv_http_on_parser_begin;
    conn->parser_setting.on_url = _uv_http_on_parser_url;
    conn->parser_setting.on_status = _uv_http_on_parser_status;
    conn->parser_setting.on_method = _uv_http_on_parser_method;
    conn->parser_setting.on_version = _uv_http_on_parser_version;
    conn->parser_setting.on_header_field = _uv_http_on_parser_header_field;
    conn->parser_setting.on_header_value = _uv_http_on_parser_header_value;
    conn->parser_setting.on_header_value_complete = _uv_http_on_parser_header_value_complete;
    conn->parser_setting.on_body = _uv_http_on_parser_body;
    conn->parser_setting.on_message_complete = _uv_http_on_parser_complete;
    llhttp_init(&conn->parser, HTTP_BOTH, &conn->parser_setting);

    if ((ret = uv_tcp_init(http->loop, &conn->client_sock)) != 0)
    {
        free(conn);
        return;
    }

    /* Save to client table. */
    ev_list_push_back(&http->client_table, &conn->c_node);

    if ((ret = uv_accept(server, (uv_stream_t*)&conn->client_sock)) != 0)
    {
        _uv_http_close_connection(http, conn, 0);
        return;
    }

    ret = uv_read_start((uv_stream_t*)&conn->client_sock, _uv_http_on_alloc, _uv_http_on_read);
    if (ret != 0)
    {
        _uv_http_close_connection(http, conn, 0);
        return;
    }

    http->cb(conn, UV_HTTP_ACCEPT, NULL, http->arg);
}

static void _uv_http_destroy_action(uv_http_action_t* action)
{
    switch (action->type)
    {
    case UV_HTTP_ACTION_SEND:
        break;

    default:
        abort();
        break;
    }

    free(action);
}

static void _uv_http_send_cb(uv_write_t* req, int status)
{
    (void)status;
    uv_http_action_t* action = container_of(req, uv_http_action_t, as.send.req);
    uv_http_conn_t* conn = action->belong;

    _uv_http_destroy_action(action);
    _uv_http_active_connection(conn);
}

static const char* _uv_http_status_code_str(int status_code)
{
    switch (status_code)
    {
    case 100: return "Continue";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 206: return "Partial Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 418: return "I'm a teapot";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    default:  return "OK";
    }
}

static void _uv_http_on_close(uv_handle_t* handle)
{
    uv_http_t* http = container_of((uv_tcp_t*)handle, uv_http_t, listen_sock);

    if (http->close_cb != NULL)
    {
        http->close_cb(http);
    }
}

static int _uv_http_active_connection_send(uv_http_conn_t* conn, uv_http_send_token_t* token)
{
    uv_buf_t buf = uv_buf_init(token->data, token->size);
    return uv_write(&token->req, (uv_stream_t*)&conn->client_sock, &buf, 1, _uv_http_send_cb);
}

static int _uv_http_gen_reply_v(uv_http_str_t* str, int status_code,
    const uv_http_str_t* body, const char* header_fmt, va_list ap)
{
    int ret;
    body = body != NULL ? body : &s_empty_str;

    ret = _uv_http_str_printf(str, "HTTP/1.1 %d %s\r\n",
        status_code, _uv_http_status_code_str(status_code));
    if (ret != 0)
    {
        return ret;
    }

    if ((ret = _uv_http_str_vprintf(str, header_fmt, ap)) != 0)
    {
        return ret;
    }

    if ((ret = _uv_http_str_printf(str, "Content-Length: %lu", body->len)) < 0)
    {
        return ret;
    }

    if ((ret = _uv_http_str_append(str, body->ptr, body->len)) != 0)
    {
        return ret;
    }

    return 0;
}

static int _uv_http_gen_reply(uv_http_str_t* str, int status_code,
    const uv_http_str_t* body, const char* header_fmt, ...)
{
    int ret;

    va_list ap;
    va_start(ap, header_fmt);
    ret = _uv_http_gen_reply_v(str, status_code, body, header_fmt, ap);
    va_end(ap);

    return ret;
}

static int _uv_http_active_connection_serve_file_once(uv_http_serve_token_t* token, uv_http_str_t* file)
{
    char etag[64];
    uv_http_fs_t* fs = token->fs;

    /* Open file */
    void* fd = fs->open(fs, file->ptr, UV_HTTP_FS_READ);
    if (fd == NULL)
    {
        return UV_ENOENT;
    }

    /* Get file information. */
    size_t size; time_t mtime;
    int flags = fs->stat(fs, file->ptr, &size, &mtime);
    if (flags == 0)
    {
        fs->close(fs, fd);
        return UV_ENOENT;
    }

    /* Check etag. */
    snprintf(etag, sizeof(etag), "\"%lld.%lld\"", (long long)mtime, (long long)size);
    if (strcasecmp(etag, token->if_none_match.ptr) == 0)
    {
        fs->close(fs, fd);
        _uv_http_gen_reply(&token->rsp, 304, NULL, "%s", token->extra_headers.ptr);
        return 0;
    }

    /* Check range. */
}

static void _uv_http_active_connection_serve_file(uv_http_serve_token_t* token, uv_http_str_t* file)
{
    /* Try to serve file. */
    int ret = _uv_http_active_connection_serve_file_once(token, file);
    if (ret == 0)
    {
        return;
    }

    /* Serve 404 page. */
    ret = _uv_http_active_connection_serve_file_once(token, &token->page404);
    if (ret == 0)
    {
        return;
    }

    /* Pure 404 response. */
    static uv_http_str_t str_not_found = { "Not found", 9, 0 };
    _uv_http_gen_reply(&token->rsp, 404, &str_not_found, "%s", token->extra_headers.ptr);
}

static int _uv_http_url_safe(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c == '.' || c == '_' || c == '-' || c == '~';
}

static size_t _uv_http_hex(const void* buf, size_t len, char* to)
{
    size_t i = 0;
    const unsigned char *p = (const unsigned char *) buf;
    const char *hex = "0123456789abcdef";

    for (; len--; p++)
    {
        to[i++] = hex[p[0] >> 4];
        to[i++] = hex[p[0] & 0x0f];
    }

    return i;
}

static int _uv_http_url_encode(const uv_http_str_t* src, uv_http_str_t* dst)
{
    size_t i;
    int ret;
    char buf[2];

    assert(src != dst);

    for (i = 0; i < src->len; i++)
    {
        char c = src->ptr[i];
        if (_uv_http_url_safe(c))
        {
            ret = _uv_http_str_append_c(dst, c);
        }
        else
        {
            _uv_http_hex(&c, 1, buf);
            ret = _uv_http_str_append(dst, buf, 2);
        }

        if (ret != 0)
        {
            return ret;
        }
    }

    return 0;
}

typedef struct uv_http_serve_dir_helper
{
    uv_http_serve_token_t*  token;
    uv_http_str_t*          path;
    uv_http_str_t*          body;
} uv_http_serve_dir_helper_t;

static uv_http_str_t _uv_http_str(const char* str)
{
    uv_http_str_t tmp;

    tmp.ptr = (char*)str;
    tmp.len = strlen(str);
    tmp.cap = 0;

    return tmp;
}

static void _uv_http_active_connection_serve_dir_on_list(const char* path, void* arg)
{
    char buf[PATH_MAX];
    uv_http_serve_dir_helper_t* helper = arg;
    uv_http_fs_t* fs = helper->token->fs;

    snprintf(buf, sizeof(buf), "%s/%s", helper->path->ptr, path);

    size_t size; time_t mtime;
    int ret = fs->stat(fs, buf, &size, &mtime);
    if (ret == 0)
    {
        return;
    }

    const char* slash = "";
    int64_t t_size = (int64_t)size;
    char sz[64];

    if (ret & UV_HTTP_FS_DIR)
    {
        slash = "/";
        t_size = -1;
        memcpy(sz, "[DIR]", 6);
    }
    else
    {
        snprintf(sz, sizeof(sz), "%llu", (unsigned long long)size);
    }

    uv_http_str_t tmp_path = _uv_http_str(path);
    uv_http_str_t encoded_path = UV_HTTP_STR_INIT;
    _uv_http_url_encode(&tmp_path, &encoded_path);

    _uv_http_str_printf(helper->body,
        "<tr>"
            "<td><a href=\"%s%s\">%s%s</a></td>"
            "<td name=%lu>%lu</td>"
            "<td name=%lld>%s</td>"
        "</tr>",
        encoded_path.ptr, slash, path, slash,
        (unsigned long)mtime, (unsigned long)mtime,
        t_size, sz);

    _uv_http_str_destroy(&encoded_path);
}

static void _uv_http_active_connection_serve_dir(uv_http_serve_token_t* token, uv_http_str_t* path)
{
    uv_http_fs_t* fs = token->fs;
    const char* sort_js_code =
        "<script>function srt(tb, sc, so, d) {"
        "var tr = Array.prototype.slice.call(tb.rows, 0),"
        "tr = tr.sort(function (a, b) { var c1 = a.cells[sc], c2 = b.cells[sc],"
        "n1 = c1.getAttribute('name'), n2 = c2.getAttribute('name'), "
        "t1 = a.cells[2].getAttribute('name'), "
        "t2 = b.cells[2].getAttribute('name'); "
        "return so * (t1 < 0 && t2 >= 0 ? -1 : t2 < 0 && t1 >= 0 ? 1 : "
        "n1 ? parseInt(n2) - parseInt(n1) : "
        "c1.textContent.trim().localeCompare(c2.textContent.trim())); });"
        "for (var i = 0; i < tr.length; i++) tb.appendChild(tr[i]); "
        "if (!d) window.location.hash = ('sc=' + sc + '&so=' + so); "
        "};"
        "window.onload = function() {"
        "var tb = document.getElementById('tb');"
        "var m = /sc=([012]).so=(1|-1)/.exec(window.location.hash) || [0, 2, 1];"
        "var sc = m[1], so = m[2]; document.onclick = function(ev) { "
        "var c = ev.target.rel; if (c) {if (c == sc) so *= -1; srt(tb, c, so); "
        "sc = c; ev.preventDefault();}};"
        "srt(tb, sc, so, true);"
        "}"
        "</script>";

    uv_http_str_t body = UV_HTTP_STR_INIT;
    _uv_http_str_printf(&body,
        "<!DOCTYPE html><html><head><title>Index of %.*s</title>%s"
        "<style>th,td {text-align: left; padding-right: 1em; "
        "font-family: monospace; }</style></head>"
        "<body><h1>Index of %.*s</h1><table cellpadding=\"0\"><thead>"
        "<tr><th><a href=\"#\" rel=\"0\">Name</a></th><th>"
        "<a href=\"#\" rel=\"1\">Modified</a></th>"
        "<th><a href=\"#\" rel=\"2\">Size</a></th></tr>"
        "<tr><td colspan=\"3\"><hr></td></tr>"
        "</thead>"
        "<tbody id=\"tb\">\n"
        "<tr><td><a href=\"..\">..</a></td>"
        "<td name=-1></td><td name=-1>[DIR]</td></tr>\n",
        (int)token->url.len, token->url.ptr, sort_js_code,
        (int)token->url.len, token->url.ptr);

    uv_http_serve_dir_helper_t helper = { token, path, &body };
    fs->ls(fs, path->ptr, _uv_http_active_connection_serve_dir_on_list, &helper);

    _uv_http_str_printf(&body,
        "</tbody><tfoot><tr><td colspan=\"3\"><hr></td></tr></tfoot>"
        "</table><address>Mongoose v.%s</address></body></html>\n", "7.8");

    _uv_http_gen_reply(&token->rsp, 200, &body, "%sContent-Type: text/html; charset=utf-8\r\n",
        token->extra_headers.ptr);
}

static void _uv_http_active_connection_serve_work(uv_work_t* req)
{
    uv_http_serve_token_t* token = container_of(req, uv_http_serve_token_t, req);
    uv_http_fs_t* fs = token->fs;
    int flags = fs->stat(fs, token->url.ptr, NULL, NULL);

    /* If it is a directory, list entry. */
    if (flags & UV_HTTP_FS_DIR)
    {
        _uv_http_active_connection_serve_dir(token, &token->url);
        return;
    }

    /* If it is a file, serve with file support. */
    _uv_http_active_connection_serve_file(token, &token->url);
}

static void _uv_http_active_connection_serve_after_work(uv_work_t* req, int status)
{
    (void)status;
    uv_http_serve_token_t* token = container_of(req, uv_http_serve_token_t, req);
    uv_http_action_t* action = container_of(token, uv_http_action_t, as.serve);
    uv_http_conn_t* conn = action->belong;

    /* Send response. */
    int ret = uv_http_send(conn, token->rsp.ptr, token->rsp.len);
    _uv_http_destroy_action(action);

    if (ret == 0)
    {
        _uv_http_active_connection(conn);
    }
    else
    {
        _uv_http_close_connection(conn->belong, conn, 1);
    }
}

static int _uv_http_active_connection_serve(uv_http_conn_t* conn, uv_http_serve_token_t* token)
{
    uv_http_t* http = conn->belong;
    return uv_queue_work(http->loop, &token->req,
        _uv_http_active_connection_serve_work,
        _uv_http_active_connection_serve_after_work);
}

static int _uv_http_active_connection(uv_http_conn_t* conn)
{
    int ret;
    auto_list_node_t* it;
    uv_http_action_t* action;

    if (ev_list_size(&conn->action_queue) != 1)
    {
        return 0;
    }

begin:
    if ((it = ev_list_pop_front(&conn->action_queue)) == NULL)
    {
        return 0;
    }
    action = container_of(it, uv_http_action_t, node);

    switch (action->type)
    {
    case UV_HTTP_ACTION_SEND:
        ret = _uv_http_active_connection_send(conn, &action->as.send);
        break;

    case UV_HTTP_ACTION_SERVE:
        ret = _uv_http_active_connection_serve(conn, &action->as.serve);
        break;

    default:
        abort();
        break;
    }

    if (ret != 0)
    {
        _uv_http_destroy_action(action);
        goto begin;
    }

    return ret;
}

int uv_http_init(uv_http_t* http, uv_loop_t* loop)
{
    int ret;
    memset(http, 0, sizeof(*http));

    ev_list_init(&http->client_table);

    http->loop = loop;
    if ((ret = uv_tcp_init(loop, &http->listen_sock)) != 0)
    {
        return ret;
    }

    return 0;
}

void uv_http_exit(uv_http_t* http, uv_http_close_cb cb)
{
    /* Close all connections. */
    auto_list_node_t* it;
    while ((it = ev_list_begin(&http->client_table)) != NULL)
    {
        uv_http_conn_t* conn = container_of(it, uv_http_conn_t, c_node);
        _uv_http_close_connection(http, conn, 1);
    }

    /* Close http. */
    uv_close((uv_handle_t*)&http->listen_sock, _uv_http_on_close);
    http->close_cb = cb;
}

int uv_http_listen(uv_http_t* http, const char* url, uv_http_cb cb, void* arg)
{
    int ret;
    if ((ret = _uv_http_bind_address(http, url)) != 0)
    {
        return ret;
    }

    if ((ret = uv_listen((uv_stream_t*)&http->listen_sock, 1024, _uv_http_on_listen)) != 0)
    {
        return ret;
    }

    http->cb = cb != NULL ? cb : _uv_http_default_cb;
    http->arg = arg;

    return 0;
}

int uv_http_send(uv_http_conn_t* conn, const void* data, size_t size)
{
    size_t malloc_size = sizeof(uv_http_action_t) + size;
    uv_http_action_t* action = malloc(malloc_size);
    if (action == NULL)
    {
        return UV_ENOMEM;
    }

    action->type = UV_HTTP_ACTION_SEND;
    action->belong = conn;

    action->as.send.size = size;
    action->as.send.data = (char*)(action + 1);
    memcpy(action->as.send.data, data, size);

    ev_list_push_back(&conn->action_queue, &action->node);

    return _uv_http_active_connection(conn);
}

static int _uv_http_reply_v(uv_http_conn_t* conn, int status_code,
    const uv_http_str_t* body, const char* header_fmt, va_list ap)
{
    int ret;

    uv_http_str_t dat = UV_HTTP_STR_INIT;
    if ((ret = _uv_http_gen_reply_v(&dat, status_code, body, header_fmt, ap)) != 0)
    {
        _uv_http_str_destroy(&dat);
        return ret;
    }

    ret = uv_http_send(conn, dat.ptr, dat.len);
    _uv_http_str_destroy(&dat);

    return ret;
}

int uv_http_reply(uv_http_conn_t* conn, int status_code,
    const void* body, size_t body_sz, const char* header_fmt, ...)
{
    int ret;
    uv_http_str_t body_str = { (char*)body, body_sz, 0 };

    va_list ap;
    va_start(ap, header_fmt);
    ret = _uv_http_reply_v(conn, status_code, &body_str, header_fmt, ap);
    va_end(ap);

    return ret;
}

static void _uv_http_fs_release(struct uv_http_fs* self)
{
    (void)self;
    return;
}

static int _uv_http_fs_stat(struct uv_http_fs* self, const char* path, size_t* size, time_t* mtime)
{
    (void)self;
#if defined(_WIN32)
    struct _stat st;
    if (_stat(path, &st) != 0)
    {
        return 0;
    }
    int is_dir = st.st_mode & _S_IFDIR;
#else
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return 0;
    }
    int is_dir = S_ISDIR(st.st_mode);
#endif

    if (size != NULL)
    {
        *size = st.st_size;
    }
    if (mtime != NULL)
    {
        *mtime = st.st_mtime;
    }
    return UV_HTTP_FS_READ | UV_HTTP_FS_WRITE | (is_dir ? UV_HTTP_FS_DIR : 0);
}

static void _uv_http_fs_ls(struct uv_http_fs* self, const char* path, void (*cb)(const char* path, void* arg), void* arg)
{
    (void)self;
#if defined(_WIN32)

    uv_http_str_t fix_path = UV_HTTP_STR_INIT;
    _uv_http_str_printf(&fix_path, "%s/*", path);

    WIN32_FIND_DATAA find_data;
    HANDLE dp = FindFirstFileA(fix_path.ptr, &find_data);
    _uv_http_str_destroy(&fix_path);
    if (dp == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
        {
            continue;
        }
        cb(find_data.cFileName, arg);
    } while (FindNextFileA(dp, &find_data));

    FindClose(dp);

#else
    DIR* dir;
    struct dirent* dp;

    if ((dir = opendir(path)) == NULL)
    {
        return;
    }

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
        {
            continue;
        }
        cb(dp->d_name, arg);
    }
    closedir(dir);
#endif
}

static void* _uv_http_fs_open(struct uv_http_fs* self, const char* path, int flags)
{
    (void)self;
    const char* mode = flags == UV_HTTP_FS_READ ? "rb" : "a+b";
    return (void*)fopen(path, mode);
}

static void _uv_http_fs_close(struct uv_http_fs* self, void* fd)
{
    (void)self;
    fclose((FILE*)fd);
}

static int _uv_http_fs_read(struct uv_http_fs* self, void* fd, void* buf, size_t size)
{
    (void)self;
    return fread(buf, 1, size, (FILE*)fd);
}

static int _uv_http_fs_write(struct uv_http_fs* self, void* fd, void* buf, size_t size)
{
    (void)self;
    return fwrite(buf, 1, size, (FILE*)fd);
}

static int _uv_http_fs_seek(struct uv_http_fs* self, void* fd, size_t offset)
{
    (void)self;
    if (fseek(fd, offset, SEEK_SET) != 0)
    {
        return uv_translate_sys_error(errno);
    }
    return 0;
}

int uv_http_serve_dir(uv_http_conn_t* conn, uv_http_message_t* msg,
    uv_http_serve_cfg_t* cfg)
{
    static uv_http_fs_t s_builtin_fs = {
        _uv_http_fs_release,
        _uv_http_fs_stat,
        _uv_http_fs_ls,
        _uv_http_fs_open,
        _uv_http_fs_close,
        _uv_http_fs_read,
        _uv_http_fs_write,
        _uv_http_fs_seek,
    };

    char* pos;
    uv_http_str_t* if_none_match = uv_http_get_header(msg, "If-None-Match");
    if_none_match = if_none_match != NULL ? if_none_match : &s_empty_str;
    uv_http_str_t* range = uv_http_get_header(msg, "Range");
    range = range != NULL ? range : &s_empty_str;

    size_t root_path_len = strlen(cfg->root_path);
    size_t ssi_pattern_len = cfg->ssi_pattern != NULL ? strlen(cfg->ssi_pattern) : 0;
    size_t extra_headers_len = cfg->extra_headers != NULL ? strlen(cfg->extra_headers) : 0;
    size_t page404_len = cfg->page404 != NULL ? strlen(cfg->page404) : 0;
    size_t if_none_match_len = if_none_match->len ;
    size_t range_len = range->len;

    size_t malloc_size = sizeof(uv_http_action_t) + msg->url.len + 1 + root_path_len + 1
        + ssi_pattern_len + 1 + extra_headers_len + 1 + page404_len + 1
        + if_none_match_len + 1 + range_len + 1;
    uv_http_action_t* action = malloc(malloc_size);
    if (action == NULL)
    {
        return UV_ENOMEM;
    }
    memset(&action->as.serve, 0, sizeof(action->as.serve));

    action->belong = conn;
    action->type = UV_HTTP_ACTION_SERVE;

    pos = (char*)(action + 1);
    action->as.serve.url.cap = 0;
    action->as.serve.url.len = msg->url.len;
    action->as.serve.url.ptr = pos;
    memcpy(action->as.serve.url.ptr, msg->url.ptr, msg->url.len);
    action->as.serve.url.ptr[action->as.serve.url.len] = '\0';
    pos += msg->url.len + 1;

    action->as.serve.root_path.cap = 0;
    action->as.serve.root_path.len = root_path_len;
    action->as.serve.root_path.ptr = pos;
    memcpy(action->as.serve.root_path.ptr, cfg->root_path, root_path_len);
    action->as.serve.root_path.ptr[root_path_len] = '\0';
    pos += root_path_len + 1;

    action->as.serve.ssi_pattern.cap = 0;
    action->as.serve.ssi_pattern.len = ssi_pattern_len;
    action->as.serve.ssi_pattern.ptr = pos;
    memcpy(action->as.serve.ssi_pattern.ptr, cfg->ssi_pattern, ssi_pattern_len);
    action->as.serve.ssi_pattern.ptr[ssi_pattern_len] = '\0';
    pos += ssi_pattern_len + 1;

    action->as.serve.extra_headers.cap = 0;
    action->as.serve.extra_headers.len = extra_headers_len;
    action->as.serve.extra_headers.ptr = pos;
    memcpy(action->as.serve.extra_headers.ptr, cfg->extra_headers, extra_headers_len);
    action->as.serve.extra_headers.ptr[extra_headers_len] = '\0';
    pos += extra_headers_len + 1;

    action->as.serve.page404.cap = 0;
    action->as.serve.page404.len = page404_len;
    action->as.serve.page404.ptr = pos;
    memcpy(action->as.serve.page404.ptr, cfg->page404, page404_len);
    action->as.serve.page404.ptr[page404_len] = '\0';
    pos += page404_len + 1;

    action->as.serve.if_none_match.cap = 0;
    action->as.serve.if_none_match.len = if_none_match_len;
    action->as.serve.if_none_match.ptr = pos;
    memcpy(action->as.serve.if_none_match.ptr, if_none_match->ptr, if_none_match_len);
    action->as.serve.if_none_match.ptr[if_none_match_len] = '\0';
    pos += if_none_match_len + 1;

    action->as.serve.range.cap = 0;
    action->as.serve.range.len = range_len;
    action->as.serve.range.ptr = pos;
    memcpy(action->as.serve.range.ptr, range->ptr, range_len);
    action->as.serve.range.ptr[range_len] = '\0';
    pos += range_len + 1;

    action->as.serve.fs = cfg->fs != NULL ? cfg->fs : &s_builtin_fs;
    action->as.serve.rsp = (uv_http_str_t)UV_HTTP_STR_INIT;

    ev_list_push_back(&conn->action_queue, &action->node);
    return _uv_http_active_connection(conn);
}

uv_http_str_t* uv_http_get_header(uv_http_message_t* msg, const char* name)
{
    size_t i;
    size_t name_len = strlen(name);
    uv_http_header_t* hdr;

    for (i = 0; i < msg->header_len; i++)
    {
        hdr = &msg->headers[i];
        if (hdr->name.len == name_len &&
            strncasecmp(hdr->name.ptr, name, name_len) == 0)
        {
            return &msg->headers[i].value;
        }
    }

    return NULL;
}
