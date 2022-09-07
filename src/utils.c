#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "utils.h"

static void _BINARY_BM_PreBmBc(int32_t* bc, size_t bcLen, const uint8_t* key, size_t keyLen)
{
    size_t i = 0;
    for (i = 0; i < bcLen; i++)
    {
        bc[i] = (int32_t)keyLen;
    }

    for (i = 0; i < keyLen - 1; i++)
    {
        bc[key[i]] = (int32_t)(keyLen - 1 - i);
    }
}

static int _BUFFER_BM_IsPrefix(const uint8_t *word, int32_t wordlen, int32_t pos)
{
    int32_t i;
    int32_t suffixlen = wordlen - pos;

    for (i = 0; i < suffixlen; i++)
    {
        if (word[i] != word[pos + i])
        {
            return 0;
        }
    }
    return 1;
}

static int32_t _BUFFER_BM_SuffixLength(const uint8_t *word, int32_t wordlen, int32_t pos)
{
    int32_t i = 0;
    // increment suffix length i to the first mismatch or beginning
    // of the word
    for (i = 0; (word[pos - i] == word[wordlen - 1 - i]) && (i < pos); i++);
    return i;
}

static int32_t _BINARY_BM_PreFSM(int32_t* fsm, const uint8_t* key, int32_t keyLen)
{
    int32_t p = 0;
    int32_t last_prefix_index = keyLen - 1;

    // first loop, prefix pattern
    for (p = keyLen - 1; p >= 0; p--)
    {
        if (_BUFFER_BM_IsPrefix(key, keyLen, p + 1))
        {
            last_prefix_index = p + 1;
        }
        fsm[p] = (keyLen - 1 - p) + last_prefix_index;
    }

    // this is overly cautious, but will not produce anything wrong
    // second loop, suffix pattern
    for (p = 0; p < keyLen - 1; p++)
    {
        int32_t slen = _BUFFER_BM_SuffixLength(key, keyLen, p);
        if (key[p - slen] != key[keyLen - 1 - slen])
        {
            fsm[keyLen - 1 - slen] = keyLen - 1 - p + slen;
        }
    }

    return 0;
}

static inline int32_t _BINARY_BM_Max(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

int aeda_find(const void* data, size_t dataLen, const void* key, size_t keyLen, int32_t* fsm, size_t fsmLen)
{
    if (data == NULL || dataLen <= 0 || key == NULL || keyLen <= 0 || fsm == NULL || fsmLen < keyLen)
    {
        return -1;
    }
    const uint8_t* p_data = data;
    const uint8_t* p_key = key;

    /************************************************************************/
    /* prepare                                                              */
    /************************************************************************/
    int32_t bmBc[(1 << (sizeof(uint8_t) * 8))];
    _BINARY_BM_PreBmBc(bmBc, ARRAY_SIZE(bmBc), p_key, keyLen);
    _BINARY_BM_PreFSM(fsm, p_key, (int32_t)keyLen);

    /************************************************************************/
    /* search                                                               */
    /************************************************************************/
    size_t i = keyLen - 1;
    while (i < dataLen)
    {
        int32_t j = (int32_t)(keyLen - 1);
        while (j >= 0 && (p_data[i] == p_key[j]))
        {
            --i;
            --j;
        }
        if (j < 0)
        {
            return (int)(i + 1);
        }

        i += _BINARY_BM_Max(bmBc[p_data[i]], fsm[j]);
    }

    return -1;
}

const char* get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

const char* auto_strerror(int errcode, char* buffer, size_t size)
{
#if defined(_WIN32)
    strerror_s(buffer, size, errcode);
#else
    strerror_r(errcode, buffer, size);
#endif
    return buffer;
}

char* auto_strdup(const char* s)
{
#if defined(_WIN32)
    return _strdup(s);
#else
    return strdup(s);
#endif
}

int auto_readfile(const char* path, void** data, size_t* size)
{
    FILE* exe;
    int errcode;

#if defined(_WIN32)
    errcode = fopen_s(&exe, path, "rb");
#else
    exe = fopen(path, "rb");
    errcode = errno;
#endif

    if (exe == NULL)
    {
        return errcode;
    }

    fseek(exe, 0L, SEEK_END);
    size_t file_size = ftell(exe);
    fseek(exe, 0L, SEEK_SET);

    *data = malloc(file_size);
    fread(*data, file_size, 1, exe);
    fclose(exe);

    *size = file_size;
    return 0;
}
