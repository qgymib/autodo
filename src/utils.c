#include <uv.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include "utils.h"

#if defined(_WIN32)
#include <shlwapi.h>
#else
#include <sys/stat.h>
#endif

#define PROBE       "AUTOMATION"

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

const char* get_filename(const char *filename)
{
    const char* pos = filename;
    for (; *filename; ++filename)
    {
        if (*filename == '\\' || *filename == '/')
        {
            pos = filename + 1;
        }
    }
    return pos;
}

const char* atd_strerror(int errcode, char* buffer, size_t size)
{
#if defined(_WIN32)
    strerror_s(buffer, size, errcode);
#else
    strerror_r(errcode, buffer, size);
#endif
    return buffer;
}

char* atd_strdup(const char* s)
{
#if defined(_WIN32)
    return _strdup(s);
#else
    return strdup(s);
#endif
}

int atd_readfile(const char* path, void** data, size_t* size)
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
        *data = NULL;
        *size = 0;
        return errcode;
    }

    fseek(exe, 0L, SEEK_END);
    size_t file_size = ftell(exe);
    fseek(exe, 0L, SEEK_SET);

    *data = malloc(file_size);
    size_t ret = fread(*data, file_size, 1, exe);
    fclose(exe);

    if (ret != 1)
    {
        free(*data);
        *data = NULL;
        *size = 0;
        return -1;
    }

    *size = file_size;
    return 0;
}

int atd_read_self(void** data, size_t* size)
{
    char buffer[1024];
    size_t buffer_sz = sizeof(buffer);
    uv_exepath(buffer, &buffer_sz);

    return atd_readfile(buffer, data, size);
}

void auto_init_probe(atd_probe_t* probe)
{
    probe->probe[0] = 0;
    probe->probe[1] = 128;
    probe->probe[2] = '=';
    memcpy(&probe->probe[3], "AUTOMATION", 10);
    probe->probe[13] = '=';
    probe->probe[14] = 128;
    probe->probe[15] = 0;

    size_t i;
    for (i = 16; i < sizeof(probe->probe); i += 16)
    {
        memcpy(&probe->probe[i], &probe->probe[0], 16);
    }
}

int atd_read_self_script(void** data, size_t* size)
{
    int ret;
    void* content; size_t content_size;

    atd_probe_t probe_data;
    auto_init_probe(&probe_data);

    if ((ret = atd_read_self(&content, &content_size)) != 0)
    {
        *data = NULL;
        *size = 0;
        return ret;
    }

    int32_t fsm[sizeof(probe_data)];
    int script_offset = aeda_find(content, content_size, &probe_data, sizeof(probe_data),
        fsm, sizeof(probe_data));
    if (script_offset < 0)
    {
        *data = NULL;
        *size = 0;
        free(content);
        return 0;
    }

    script_offset += sizeof(probe_data);

    *size = content_size - script_offset;
    *data = malloc(*size);
    memcpy(*data, (char*)content + script_offset, *size);

    free(content);

    return 0;
}

int atd_read_self_exec(void** data, size_t* size)
{
    int ret;

    atd_probe_t probe_data;
    auto_init_probe(&probe_data);

    if ((ret = atd_read_self(data, size)) != 0)
    {
        return ret;
    }

    int32_t fsm[sizeof(probe_data)];
    int script_offset = aeda_find(*data, *size, &probe_data, sizeof(probe_data),
        fsm, sizeof(probe_data));
    if (script_offset > 0)
    {
        *size = script_offset;
    }

    return 0;
}

static int _write_executable(lua_State* L, const char* dst)
{
    FILE* dst_file;
    int errcode;
    char errbuf[1024];

#if defined(_WIN32)
    errcode = fopen_s(&dst_file, dst, "wb");
#else
    dst_file = fopen(dst, "wb");
    errcode = errno;
#endif

    (void)errcode;
    if (dst_file == NULL)
    {
        return luaL_error(L, "open `%s` failed: %s(%d).", dst,
                          atd_strerror(errno, errbuf, sizeof(errbuf)), errno);
    }

    {
        void* exe_data; size_t exe_size;
        atd_read_self_exec(&exe_data, &exe_size);
        fwrite(exe_data, exe_size, 1, dst_file);
    }

    {
        atd_probe_t probe;
        auto_init_probe(&probe);
        fwrite(&probe, sizeof(probe), 1, dst_file);
    }

    {
        size_t size;
        const char* data = lua_tolstring(L, -1, &size);
        fwrite(data, size, 1, dst_file);
    }

    fclose(dst_file);

#if !defined(_WIN32)
    chmod(dst, 0777);
#endif

    return 0;
}

static int _on_dump_compile_script(lua_State *L, const void *p, size_t sz, void *ud)
{
    (void)L;
    luaL_Buffer* buf = ud;
    luaL_addlstring(buf, p, sz);
    return 0;
}

int atd_compile_script(lua_State* L, const char* src, const char* dst)
{
    int sp = lua_gettop(L);

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    /* SP + 2 */
    int ret = luaL_loadfile(L, src);
    if (ret == LUA_ERRFILE)
    {
        return luaL_error(L, "open `%s` failed.", src);
    }
    if (ret != LUA_OK)
    {
        return lua_error(L);
    }

    lua_dump(L, _on_dump_compile_script, &buf, 0);
    luaL_pushresult(&buf);

    ret = _write_executable(L, dst);

    lua_settop(L, sp);
    return ret;
}

int atd_isabs(const char* path)
{
    /* linux */
#if defined(_WIN32)
    return !PathIsRelativeA(path);
#else
    return path[0] == '/';
#endif
}
