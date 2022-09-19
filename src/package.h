#ifndef __AUTO_PACKAGE_H__
#define __AUTO_PACKAGE_H__

#include "lua/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Package downloader and loader.
 * @param[in] L     Lua VM.
 * @return
 */
AUTO_LOCAL int atd_package_loader(lua_State* L);

#ifdef __cplusplus
}
#endif
#endif
