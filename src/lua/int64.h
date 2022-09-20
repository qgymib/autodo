#ifndef __AUTO_LUA_INT64_H__
#define __AUTO_LUA_INT64_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

AUTO_LOCAL int api_int64_push_value(lua_State *L, int64_t value);

AUTO_LOCAL int api_int64_get_value(lua_State *L, int idx, int64_t* value);

#ifdef __cplusplus
}
#endif

#endif
