#ifndef __AUTO_PACKAGE_H__
#define __AUTO_PACKAGE_H__

#include <uv.h>
#include <cJSON.h>
#include "lua/api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct auto_lua_module
{
    atd_list_node_t     node;   /**< List node */
    struct
    {
        uv_lib_t        lib;    /**< Shared library handle */
        lua_CFunction   entry;  /**< Shared library entrypoint */
        char*           path;   /**< Shared library path */
    } data;
} auto_lua_module_t;

/**
 * @brief Inject custom module loader.
 * @param[in] L     Lua VM.
 * @return          Boolean.
 */
AUTO_LOCAL int auto_inject_loader(lua_State* L);

/**
 * @brief Parser module into path / name / option
 * @param[in] raw   Argument for `require()`
 * @param[out] path Module path. Must call `free()` when no longer used.
 * @param[out] name Module name. Must call `free()` when no longer used.
 * @param[out] opt  Module options. Must call `free()` when no longer used.
 */
AUTO_LOCAL void auto_require_split(const char* raw, char** path, char** name, cJSON** opt);

#ifdef __cplusplus
}
#endif
#endif
