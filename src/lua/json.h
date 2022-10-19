#ifndef __AUTO_LUA_JSON_H__
#define __AUTO_LUA_JSON_H__

#include "api.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Push a JSON parser on top of stack.
 * @param[in] L     Lua VM.
 * @return          Always 1.
 */
AUTO_LOCAL int auto_lua_json(lua_State* L);

/**
 * @brief Convert Lua table into JSON object.
 * @param[in] L     Lua VM.
 * @param[in] idx   Table index.
 * @return          JSON object.
 */
AUTO_LOCAL cJSON* auto_lua_json_from_table(lua_State* L, int idx);

#ifdef __cplusplus
}
#endif

#endif
