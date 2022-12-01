#ifndef __AUTO_UTILS_HTTP_SERVER_H__
#define __AUTO_UTILS_HTTP_SERVER_H__

#include <uv.h>
#include <llhttp.h>
#include "utils/list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Static initializer for #uv_http_str_t.
 */
#define UV_HTTP_STR_INIT    { NULL, 0 }

typedef struct uv_http_s uv_http_t;

typedef enum uv_http_event
{
    UV_HTTP_ACCEPT,
    UV_HTTP_MESSAGE,
    UV_HTTP_CLOSE,
} uv_http_event_t;

typedef enum uv_http_fs_flag
{
    UV_HTTP_FS_READ     = 1,
    UV_HTTP_FS_WRITE    = 2,
    UV_HTTP_FS_DIR      = 4,
} uv_http_fs_flag_t;

typedef struct uv_http_str
{
    char*                   ptr;            /**< String address. */
    size_t                  len;            /**< String length, not including NULL terminator. */
} uv_http_str_t;

typedef struct
{
    uv_http_str_t           name;           /**< Header name. */
    uv_http_str_t           value;          /**< Header value. */
} uv_http_header_t;

typedef struct uv_http_message_s
{
    uv_http_str_t           method;         /**< HTTP method. */
    uv_http_str_t           url;            /**< HTTP url. */
    uv_http_str_t           status;         /**< HTTP status. */
    uv_http_str_t           version;        /**< HTTP version. */

    uv_http_header_t*       headers;        /**< HTTP header array. */
    size_t                  header_sz;      /**< HTTP header array size. */
    size_t                  header_cap;     /**< HTTP header array capacity. */

    uv_http_str_t           body;           /**< HTTP body. */
} uv_http_message_t;

typedef struct uv_http_conn_s
{
    auto_list_node_t        c_node;         /**< Node for #uv_http_t::client_table */

    uv_http_t*              belong;         /**< Http instance. */
    uv_tcp_t                client_sock;    /**< Client socket. */

    llhttp_t                parser;         /**< Http parser */
    llhttp_settings_t       parser_setting; /**< Http parser settings */
    uv_http_message_t*      on_parsing;     /**< The message we are processing. */

    auto_list_t             action_queue;   /**< Action queue. */
} uv_http_conn_t;

typedef struct uv_http_fs
{
    /**
     * @brief This instance is no longer needed.
     * @param[in] self  Filesystem instance.
     */
    void (*release)(struct uv_http_fs* self);

    int (*stat)(struct uv_http_fs* self, const char* path, size_t* size, time_t* mtime);
    void (*ls)(struct uv_http_fs* self, const char* path, void (*cb)(const char* path, void* arg), void* arg);
    void* (*open)(struct uv_http_fs* self, const char* path, int flags);
    void (*close)(struct uv_http_fs* self, void* fd);
    int (*read)(struct uv_http_fs* self, void* fd, void* buf, size_t size);
    int (*write)(struct uv_http_fs* self, void* fd, void* buf, size_t size);
    int (*seek)(struct uv_http_fs* self, void* fd, size_t offset);
} uv_http_fs_t;

typedef struct uv_http_serve_cfg
{
    const char*         root_path;          /**< Web root directory, must be non-NULL. */
    const char*         ssi_pattern;        /**< (Optional) SSI file name pattern. */
    const char*         extra_headers;      /**< (Optional) Extra HTTP headers to add in responses. */
    const char*         page404;            /**< (Optional) Path to the 404 page. */
    uv_http_fs_t*       fs;                 /**< (Optional) Filesystem instance. */
} uv_http_serve_cfg_t;

/**
 * @brief HTTP event callback.
 * @param[in] conn  Connection.
 * @param[in] evt   (#uv_http_conn_t) Event.
 * @param[in] data  Event data.
 * @param[in] arg   User defined argument.
 */
typedef void (*uv_http_cb)(uv_http_conn_t* conn, int evt, void* data, void* arg);

/**
 * @brief HTTP close callback.
 * @param[in] http  HTTP component.
 */
typedef void (*uv_http_close_cb)(uv_http_t* http);

struct uv_http_s
{
    uv_loop_t*              loop;           /**< Event loop. */
    uv_tcp_t                listen_sock;    /**< Listening socket. */

    auto_list_t             client_table;   /**< (#uv_http_conn_t) Connection table. */

    uv_http_cb              cb;             /**< Event callback. */
    void*                   arg;            /**< User defined data passed to cb. */

    uv_http_close_cb        close_cb;       /**< Close callback. */
};

/**
 * @brief Create HTTP component.
 * @param[in] loop  Event loop.
 * @return          HTTP Component instance.
 */
int uv_http_init(uv_http_t* http, uv_loop_t* loop);

/**
 * @brief Close HTTP component.
 * @param[in] http  HTTP Component instance.
 * @param[in] cb    Close callback.
 */
void uv_http_exit(uv_http_t* http, uv_http_close_cb cb);

/**
 * @brief Do http listen.
 * @param[in] http  HTTP Component instance.
 * @param[in] url   Listen URL.
 * @param[in] cb    Event callback.
 * @param[in] arg   User defined data passed to callback.
 * @return          UV error code.
 */
int uv_http_listen(uv_http_t* http, const char* url, uv_http_cb cb, void* arg);

/**
 * @brief Send data on http connection.
 * @param[in] conn  HTTP connection.
 * @param[in] data  Data.
 * @param[in] size  Data size.
 * @return          UV error code.
 */
int uv_http_send(uv_http_conn_t* conn, const void* data, size_t size);

/**
 * @brief Send HTTP response.
 * @param[in] conn          HTTP connection.
 * @param[in] status_code   HTTP response code.
 * @param[in] headers       Extra headers. If not NULL, must end with `\r\n`.
 * @param[in] body          Body data.
 * @param[in] body_sz       Body length in bytes.
 * @return                  UV error code.
 */
int uv_http_reply(uv_http_conn_t* conn, int status_code, const char* headers,
    const void* body, size_t body_sz);

/**
 * @brief Generate serve dir response message.
 * @param[in] conn  HTTP connection.
 * @param[in] msg   HTTP incoming message.
 * @param[in] cfg   Serve dir options.
 * @return          UV error code.
 */
int uv_http_serve_dir(uv_http_conn_t* conn, uv_http_message_t* msg,
    uv_http_serve_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#endif
