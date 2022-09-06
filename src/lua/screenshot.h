#ifndef __AUTO_LUA_SCREENSHOT_H__
#define __AUTO_LUA_SCREENSHOT_H__

#include "api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Take screenshot (png) and push onto top of \p L.
 * @param[in] L     Lua VM.
 * @return          1.
 */
int auto_lua_take_screenshot(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
