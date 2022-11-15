#ifndef __AUTO_UTILS_FST_H__
#define __AUTO_UTILS_FST_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum auto_fts_flag
{
    AUTO_FTS_POST_ORDER = 1,
    AUTO_FTS_NO_REG     = 2,
    AUTO_FTS_NO_DIR     = 4,
} auto_fts_flag_t;

struct auto_fts_s;
typedef struct auto_fts_s auto_fts_t;

typedef struct auto_fts_ent
{
    char*   name;
    size_t  name_len;
} auto_fts_ent_t;

/**
 * @brief Open Filesystem Traversing Stream.
 * @param[in] path  Directory path.
 * @return          Token.
 */
auto_fts_t* auto_fts_open(const char* path, int flags);

/**
 * @brief Close token.
 * @param[in] self  Token.
 */
void auto_fts_close(auto_fts_t* self);

/**
 * @brief Get next entry.
 * @param[in] self  Token.
 * @return          Entry.
 */
auto_fts_ent_t* auto_fts_read(auto_fts_t* self);

#ifdef __cplusplus
}
#endif

#endif
