#ifndef __AUTO_REQUIRE_FROM_FILE_H__
#define __AUTO_REQUIRE_FROM_FILE_H__

#include "loader.h"
#include "lua/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load lua module from local filesystem.
 *
 * The module name can be one of:
 * + module
 * + module:[opt]
 * + url/module
 * + url/module:[opt]
 * + path/module
 * + path/module:[opt]
 *
 * The module path rule is:
 * 1. All module must in the `.autodo/module` directory the same as launcher script.
 * 2. Only treat `slash` as directory identification, not the `dot`.
 *
 * The [opt] is a json string that describe the attribute of module.
 *
 * @see https://www.lua.org/manual/5.4/manual.html#pdf-require
 * @see https://semver.org/
 * @param[in] L         Lua VM. The stack layout must match the requirement of `package.searchers`.
 * @param[out] module   Shared library information.
 * @return              Boolean.
 */
AUTO_LOCAL int auto_load_local_module(lua_State* L, auto_lua_module_t* module);

#ifdef __cplusplus
}
#endif

#endif
