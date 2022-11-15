#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "list.h"
#include "map.h"
#include "fts.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <dirent.h>
#endif

typedef struct auto_fts_child
{
    auto_map_node_t     node;

    int                 is_dir;
    size_t              name_len;

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4200)
#endif
    char                name[];
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
} auto_fts_child_t;

typedef struct auto_fts_record
{
    auto_list_node_t    node;

    auto_map_t          child_unk_table;
    auto_map_t          child_dir_table;
    auto_map_t          child_reg_table;

    size_t              path_len;

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4200)
#endif
    char                path[];
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
} auto_fts_record_t;

struct auto_fts_s
{
    auto_list_t         dir_queue;
    auto_fts_ent_t      cache;

    int                 flags;

    size_t              root_path_len;  /**< length of root_path without NULL */

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4200)
#endif
    char                root_path[];
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
};

static int _fts_read_record(auto_fts_t* self, auto_fts_record_t* rec)
{
#if defined(_WIN32)

    size_t buf_sz = rec->path_len + 3;
    char* buf = malloc(buf_sz);
    snprintf(buf, buf_sz, "%s/*", rec->path);

    WIN32_FIND_DATAA find_data;
    HANDLE dp = FindFirstFileA(buf, &find_data);
    free(buf);
    if (dp == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    do
    {
        if (find_data.cFileName[0] == '.' && (find_data.cFileName[1] == '\0' || (find_data.cFileName[1] == '.' && find_data.cFileName[2] == '\0')))
        {
            continue;
        }

        /* Ignore file if necessary. */
        if ((self->flags & AUTO_FTS_NO_REG) && (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            continue;
        }

        size_t name_len = strlen(find_data.cFileName);
        size_t malloc_size = sizeof(auto_fts_child_t) + name_len + 1;
        auto_fts_child_t* child = malloc(malloc_size);
        child->is_dir = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        child->name_len = name_len;
        memcpy(child->name, find_data.cFileName, name_len + 1);

        if (child->is_dir)
        {
            ev_map_insert(&rec->child_dir_table, &child->node);
        }
        else
        {
            ev_map_insert(&rec->child_reg_table, &child->node);
        }
    } while (FindNextFileA(dp, &find_data));

    FindClose(dp);

#else

    DIR* dp = opendir(rec->path);
    if (dp == NULL)
    {
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dp)) != NULL)
    {
        /* Ignore self and upper folder. */
        if ((entry->d_name[0] == '.') && (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
        {
            continue;
        }

        /* Ignore file if necessary. */
        if ((self->flags & AUTO_FTS_NO_REG) && entry->d_type == DT_REG)
        {
            continue;
        }

        /* Store child record. */
        size_t d_name_len = strlen(entry->d_name);
        size_t malloc_size = sizeof(auto_fts_child_t) + d_name_len + 1;
        auto_fts_child_t* child = malloc(malloc_size);
        child->is_dir = entry->d_type == DT_DIR;
        child->name_len = d_name_len;
        memcpy(child->name, entry->d_name, d_name_len + 1);

        if (child->is_dir)
        {
            ev_map_insert(&rec->child_dir_table, &child->node);
        }
        else
        {
            ev_map_insert(&rec->child_reg_table, &child->node);
        }
    }
    closedir(dp);

#endif

    return 0;
}

static int _fts_cmp_child(const auto_map_node_t* key1,
    const auto_map_node_t* key2, void* arg)
{
    (void)arg;
    auto_fts_child_t* child1 = container_of(key1, auto_fts_child_t, node);
    auto_fts_child_t* child2 = container_of(key2, auto_fts_child_t, node);
    return strcmp(child1->name, child2->name);
}

auto_fts_t* auto_fts_open(const char* path, int flags)
{
    size_t root_path_size = strlen(path);
    size_t malloc_size = sizeof(auto_fts_t) + root_path_size + 1;
    auto_fts_t* self = malloc(malloc_size);

    memset(self, 0, malloc_size);
    ev_list_init(&self->dir_queue);
    self->root_path_len = root_path_size;
    memcpy(self->root_path, path, root_path_size + 1);
    self->flags = flags;

    auto_fts_record_t* rec = malloc(sizeof(auto_fts_record_t) + root_path_size + 1);
    rec->path_len = root_path_size + 1;
    memcpy(rec->path, path, root_path_size + 1);
    ev_map_init(&rec->child_unk_table, _fts_cmp_child, NULL);
    ev_map_init(&rec->child_reg_table, _fts_cmp_child, NULL);
    ev_map_init(&rec->child_dir_table, _fts_cmp_child, NULL);
    ev_list_push_back(&self->dir_queue, &rec->node);

    _fts_read_record(self, rec);

    return self;
}

static void _fts_destroy_record(auto_fts_t* self, auto_fts_record_t* rec)
{
    auto_map_node_t* it;
    ev_list_erase(&self->dir_queue, &rec->node);

    it = ev_map_begin(&rec->child_unk_table);
    while (it != NULL)
    {
        auto_fts_child_t* child = container_of(it, auto_fts_child_t, node);
        it = ev_map_next(it);
        ev_map_erase(&rec->child_unk_table, &child->node);
        free(child);
    }

    it = ev_map_begin(&rec->child_reg_table);
    while (it != NULL)
    {
        auto_fts_child_t* child = container_of(it, auto_fts_child_t, node);
        it = ev_map_next(it);
        ev_map_erase(&rec->child_reg_table, &child->node);
        free(child);
    }

    it = ev_map_begin(&rec->child_dir_table);
    while (it != NULL)
    {
        auto_fts_child_t* child = container_of(it, auto_fts_child_t, node);
        it = ev_map_next(it);
        ev_map_erase(&rec->child_dir_table, &child->node);
        free(child);
    }

    free(rec);
}

void auto_fts_close(auto_fts_t* self)
{
    auto_list_node_t* it;
    while ((it = ev_list_end(&self->dir_queue)) != NULL)
    {
        auto_fts_record_t* rec = container_of(it, auto_fts_record_t, node);
        _fts_destroy_record(self, rec);
    }

    free(self);
}

static void _fts_cleanup_cache(auto_fts_t* self)
{
    if (self->cache.name != NULL)
    {
        free(self->cache.name);
        self->cache.name = NULL;
    }
    self->cache.name_len = 0;
}

static auto_fts_ent_t* _fts_update_cache(auto_fts_t* self,
    auto_fts_record_t* rec, auto_fts_child_t* child, int free_child)
{
    self->cache.name_len = rec->path_len + 1 + child->name_len;
    self->cache.name = malloc(self->cache.name_len + 1);
    snprintf(self->cache.name, self->cache.name_len + 1, "%s/%s", rec->path, child->name);

    if (free_child)
    {
        free(child);
    }

    return &self->cache;
}

static auto_fts_ent_t* _fts_do_read(auto_fts_t* self, auto_fts_record_t* rec)
{
    auto_fts_child_t* child;
    auto_map_node_t* it;

    /* Unknown file. */
    it = ev_map_begin(&rec->child_unk_table);
    if (it != NULL)
    {
        child = container_of(it, auto_fts_child_t, node);
        ev_map_erase(&rec->child_unk_table, it);

        if ((self->flags & AUTO_FTS_NO_DIR) && child->is_dir)
        {
            free(child);
            return NULL;
        }

        return _fts_update_cache(self, rec, child, 1);
    }

    /* Regular file. */
    it = ev_map_begin(&rec->child_reg_table);
    if (it != NULL)
    {
        child = container_of(it, auto_fts_child_t, node);
        ev_map_erase(&rec->child_reg_table, it);
        return _fts_update_cache(self, rec, child, 1);
    }

    /* Directory. */
    it = ev_map_begin(&rec->child_dir_table);
    if (it != NULL)
    {
        child = container_of(it, auto_fts_child_t, node);
        ev_map_erase(&rec->child_dir_table, it);

        size_t child_path_len = rec->path_len + 1 + child->name_len;
        auto_fts_record_t* tmp = malloc(sizeof(auto_fts_record_t) + child_path_len + 1);
        tmp->path_len = child_path_len;
        snprintf(tmp->path, tmp->path_len + 1, "%s/%s", rec->path, child->name);
        ev_map_init(&tmp->child_unk_table, _fts_cmp_child, NULL);
        ev_map_init(&tmp->child_reg_table, _fts_cmp_child, NULL);
        ev_map_init(&tmp->child_dir_table, _fts_cmp_child, NULL);
        ev_list_push_back(&self->dir_queue, &tmp->node);
        _fts_read_record(self, tmp);

        /* Report directory first */
        if (!(self->flags & AUTO_FTS_POST_ORDER))
        {
            if (self->flags & AUTO_FTS_NO_DIR)
            {
                free(child);
                return NULL;
            }

            return _fts_update_cache(self, rec, child, 1);
        }

        /* Report directory after all child entry. */
        ev_map_insert(&rec->child_unk_table, &child->node);
        return NULL;
    }

    /* Empty record, cleanup */
    _fts_destroy_record(self, rec);
    return NULL;
}

auto_fts_ent_t* auto_fts_read(auto_fts_t* self)
{
    _fts_cleanup_cache(self);

    auto_fts_ent_t* ret;
    while (ev_list_size(&self->dir_queue) != 0)
    {
        auto_list_node_t* it = ev_list_end(&self->dir_queue);
        if (it == NULL)
        {
            return NULL;
        }
        auto_fts_record_t* rec = container_of(it, auto_fts_record_t, node);

        if ((ret = _fts_do_read(self, rec)) != NULL)
        {
            return ret;
        }
    }

    return NULL;
}
