#ifndef __AUTO_REQUIRE_FROM_NET_H__
#define __AUTO_REQUIRE_FROM_NET_H__

#include "loader.h"
#include "lua/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Download module from net and save to local filesystem.
 * @param[in] L         Lua VM.
 * @param[out] module   Shared library information.
 * @return              Boolean.
 */
AUTO_LOCAL int auto_load_net_module(lua_State* L, auto_lua_module_t* module);

#ifdef __cplusplus
}
#endif

#endif
