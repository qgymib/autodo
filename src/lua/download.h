#ifndef __AUTO_LUA_DOWNLOAD_H__
#define __AUTO_LUA_DOWNLOAD_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Download file from network.
 * @param[in] L     Lua VM.
 * @return          1 if success, 0 if failure.
 */
AUTO_LOCAL int auto_lua_download(lua_State* L);

#ifdef __cplusplus
}
#endif

#endif
