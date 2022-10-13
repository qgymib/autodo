#define _GNU_SOURCE
#include "from_file.h"
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <cJSON.h>

#if defined(_WIN32)
#   define MODULE_EXT   "dll"
#else
#   define MODULE_EXT   "so"
#endif

typedef struct file_record
{
    atd_list_node_t node;
    struct
    {
        size_t      size;
        char        data[];
    } data;
} file_record_t;

static char* _try_load_directly(atd_runtime_t* rt, const char* path, const char* name)
{
    char* load_path;
    asprintf(&load_path, "%s/.autodo/module/%s/%s." MODULE_EXT,
        rt->config.script_path, path, name);

    return load_path;
}

static void _get_file_list(atd_list_t* dst, atd_runtime_t* rt, const char* path, const char* name)
{
    size_t name_len = strlen(name);

    uv_fs_t req;
    uv_fs_scandir(&rt->loop, &req, path, 0, NULL);

    uv_dirent_t ent;
    while (uv_fs_scandir_next(&req, &ent) != UV_EOF)
    {
        if (ent.type != UV_DIRENT_FILE)
        {
            continue;
        }

        /* Only record file start with file name */
        if (strncmp(ent.name, name, name_len) == 0)
        {
            size_t ent_name_len = strlen(ent.name) + 1;
            file_record_t* record = malloc(sizeof(file_record_t) + ent_name_len);
            memcpy(record->data.data, ent.name, ent_name_len);
            record->data.size = ent_name_len - 1;
            ev_list_push_back(dst, &record->node);
        }
    }

    uv_fs_req_cleanup(&req);
}

static void _cleanup_file_list(atd_list_t* dst)
{
    atd_list_node_t* node;
    while ((node = ev_list_pop_front(dst)) != NULL)
    {
        file_record_t* record = container_of(node, file_record_t, node);
        free(record);
    }
}

static char* _try_load_with_options(atd_runtime_t* rt, const char* path,
    const char* name, cJSON* opt)
{
    (void)opt;

    /* Get search path. */
    char* search_dir;
    asprintf(&search_dir, "%s/.autodo/module/%s", rt->config.script_path, path);

    /* (#file_record_t) Get all files that match the module name. */
    atd_list_t file_list;
    ev_list_init(&file_list);
    _get_file_list(&file_list, rt, search_dir, name);

    // TODO Get highest matching version.

    /* Release temporary resource. */
    free(search_dir);
    _cleanup_file_list(&file_list);

    return NULL;
}

int auto_load_local_module(lua_State* L, auto_lua_module_t* module)
{
    int ret = 0;
    atd_runtime_t* rt = auto_get_runtime(L);
    /* Get require argument */
    const char* arg = lua_tostring(L, 1);

    /* Parse argument as path / name / option */
    char *path, *name; cJSON* opt;
    auto_require_split(arg, &path, &name, &opt);

    /* Get load path */
    char* load_path = (opt == NULL) ? _try_load_directly(rt, path, name)
        : _try_load_with_options(rt, path, name, opt);

    if (load_path == NULL)
    {
        ret = 0;
        goto finish;
    }

    /* Try open shared library */
    if (uv_dlopen(load_path, &module->data.lib) != 0)
    {
        ret = 0;
        goto finish;
    }

    /* Get entrypoint */
    char* entry_name = NULL;
    asprintf(&entry_name, "luaopen_%s", name);
    if (uv_dlsym(&module->data.lib, entry_name, (void**)&module->data.entry))
    {
        uv_dlclose(&module->data.lib);
        goto finish;
    }

    module->data.path = load_path;
    load_path = NULL;
    ret = 1;

finish:
    free(load_path);
    free(path);
    free(name);
    cJSON_Delete(opt);

    return ret;
}
