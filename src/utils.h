#ifndef __UTILS_H__
#define __UTILS_H__

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Get array size.
 * @param[in] x The array
 * @return      The size.s
 */
#define ARRAY_SIZE(x)   (sizeof(x) / sizeof(x[0]))

/**
 * @brief cast a member of a structure out to the containing structure.
 */
#if !defined(container_of)
#if defined(__GNUC__) || defined(__clang__)
#   define container_of(ptr, type, member)   \
        ({ \
            const typeof(((type *)0)->member)*__mptr = (ptr); \
            (type *)((char *)__mptr - offsetof(type, member)); \
        })
#else
#   define container_of(ptr, type, member)   \
        ((type *) ((char *) (ptr) - offsetof(type, member)))
#endif
#endif

/**
 * @brief Debug log to stdout.
 * @param[in] fmt   Log format.
 * @param[in] ...   Log arguments
 */
#define AUTO_DEBUG(fmt, ...)    \
    printf("[%s:%d %s] " fmt "\n", get_filename(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct auto_probe
{
    unsigned char   probe[1024];    /**< Probe */
} auto_probe_t;

void auto_init_probe(auto_probe_t* probe);

/**
 * @brief Find binary data.
 * @param[in] data      Binary data to find.
 * @param[in] dataLen   Binary data length.
 * @param[in] key       The needle data.
 * @param[in] keyLen    The needle data length.
 * @param[in] fsm       A search fsm, the length must at least \p keyLen.
 * @param[in] fsmLen    The length of \p fsm.
 * @return              The position of result data.
 */
int aeda_find(const void* data, size_t dataLen, const void* key, size_t keyLen, int32_t* fsm, size_t fsmLen);

/**
 * @brief Get filename extension
 * @param[in] filename  File name.
 * @return              Extension.
 */
const char* get_filename_ext(const char *filename);

/**
 * @brief Get filename
 * @param[in] filename  Full filename
 * @return              File name.
 */
const char* get_filename(const char *filename);

/**
 * @see [strerror_r(3)](https://man7.org/linux/man-pages/man3/strerror.3.html)
 */
const char* auto_strerror(int errcode, char* buffer, size_t size);

/**
 * @see [strdup(3)](https://man7.org/linux/man-pages/man3/strdup.3.html)
 */
char* auto_strdup(const char* s);

/**
 * @brief Read file content
 * @param[in] path  File path.
 * @param[out] data File content
 * @return          File size.
 */
int auto_readfile(const char* path, void** data, size_t* size);

/**
 * @brief Read self.
 * @param data
 * @param size
 * @return
 */
int auto_read_self(void** data, size_t* size);

int auto_read_self_script(void** data, size_t* size);

int auto_read_self_exec(void** data, size_t* size);

#ifdef __cplusplus
}
#endif

#endif
