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

#ifdef __cplusplus
extern "C" {
#endif

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

const char* get_filename_ext(const char *filename);

const char* auto_strerror(int errcode, char* buffer, size_t size);

char* auto_strdup(const char* s);

#ifdef __cplusplus
}
#endif

#endif
