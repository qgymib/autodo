#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include "mkdir.h"
#include "utils.h"

#if defined(_WIN32)

#include <direct.h>

#else

static int _mkdir(const char* dirname)
{
    const mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
    return mkdir(dirname, mode) != 0 ? errno : 0;
}

#endif

/* Make a directory; already existing dir okay */
static int maybe_mkdir(const char* path)
{
    int errcode;
    /* Try to make the directory */
    if ((errcode = _mkdir(path)) == 0)
    {
        return 0;
    }

    /* If it fails for any reason but EEXIST, fail */
    if (errcode != EEXIST)
    {
        return errcode;
    }

    /* Check if the existing path is a directory */
    return auto_isdir(path);
}

int auto_mkdir(const char* path, int parents)
{
    char* p;
    int errcode = 0;

    /* Create directory directly. */
    if (!parents)
    {
        return _mkdir(path);
    }

    /* Create a copy which need to free later. */
    char* dup_path = auto_strdup(path);
    if (dup_path == NULL)
    {
        return ENOMEM;
    }

    for (p = dup_path + 1; *p; p++)
    {
        char p_bak = *p;
        if (p_bak == '/' || p_bak == '\\')
        {
            *p = '\0';

            if ((errcode = maybe_mkdir(dup_path)) != 0)
            {
                goto finish;
            }

            *p = p_bak;
        }
    }

    /* Create last directory. */
    errcode = maybe_mkdir(dup_path);

finish:
    free(dup_path);
    return errcode;
}

int auto_isdir(const char* path)
{
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
    {
        return errno;
    }

#if defined(_WIN32)
    int ret = stat_buf.st_mode & _S_IFDIR;
#else
    int ret = S_ISDIR(stat_buf.st_mode);
#endif

    return ret ? 0 : ENOTDIR;
}

int auto_isfile(const char* path)
{
    struct stat stat_buf;
    if (stat(path, &stat_buf) != 0)
    {
        return errno;
    }

#if defined(_WIN32)
    int ret = stat_buf.st_mode & _S_IFREG;
#else
    int ret = S_ISREG(stat_buf.st_mode);
#endif

    return ret ? 0 : EISDIR;
}
