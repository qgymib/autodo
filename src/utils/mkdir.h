#ifndef __AUTO_UTILS_MKDIR_H__
#define __AUTO_UTILS_MKDIR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the DIRECTORY(ies), if they do not already exist.
 * @param[in] path      Directory path.
 * @param[in] parents   Make parent directories as needed.
 * @return              Errno.
 */
int auto_mkdir(const char* path, int parents);

/**
 * @brief Check if \p path is a directory.
 * @param[in] path  Directory path.
 * @return          0 if it is a directory, otherwise return error code.
 */
int auto_isdir(const char* path);

/**
 * @brief Check if \p path is a file.
 * @param[in] path  File path.
 * @return          0 if it is a file, otherwise return error code.
 */
int auto_isfile(const char* path);

#ifdef __cplusplus
}
#endif

#endif
