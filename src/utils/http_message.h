#ifndef __AUTO_UTILS_HTTP_MESSAGE_H__
#define __AUTO_UTILS_HTTP_MESSAGE_H__

#include "lua/api.h"

struct http_message_parser;
typedef struct http_message_parser http_message_parser_t;

typedef struct http_message
{
    auto_list_node_t        node;           /**< List node */

    char*                   url;
    size_t                  url_sz;

    char*                   status;
    size_t                  status_sz;

    char*                   method;
    size_t                  method_sz;

    char*                   version;
    size_t                  version_sz;
} http_message_t;

/**
 * @brief Create HTTP message parser.
 * @return  Parser.
 */
http_message_parser_t* http_message_parser_create(void);

/**
 * @brief Parser message.
 * @param[in] parser    Message parser.
 * @param[in] data      Data to parser.
 * @param[in] len       Data length in bytes.
 * @param[out] msg      HTTP message.
 * @return              0 if no error, otherwise failure.
 */
int http_message_parser_execute(http_message_parser_t* parser, const char* data, size_t len, http_message_t** msg);

/**
 * @brief Destroy http message parser.
 * @param parser
 */
void http_message_parser_destroy(http_message_parser_t* parser);

/**
 * @brief Get header field.
 * @param[in] msg       HTTP message.
 * @param[in] name      Field name.
 * @param[out] length   Array length.
 * @return              Field value array.
 */
char** http_message_get_header(http_message_t* msg, const char* name, size_t* length);

/**
 * @brief Destroy HTTP message.
 * @param[in] msg       HTTP message.
 */
void http_message_destroy(http_message_t* msg);

/**
 * @brief Print HTTP message into Lua table at index \p idx.
 * @param[in] L         Lua VM.
 * @param[in] idx       Table index.
 * @param[in] msg       HTTP message.
 * @return              Always 0.
 */
int http_message_print(lua_State* L, int idx, http_message_t* msg);

#endif
