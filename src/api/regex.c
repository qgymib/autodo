/**
 * This macro must be defined before including pcre2.h. For a program that uses
 * only one code unit width, it makes it possible to use generic function names
 * such as pcre2_compile().
 */
#define PCRE2_CODE_UNIT_WIDTH 8
/**
 * Statically link PCRE2
 */
#define PCRE2_STATIC
#include <pcre2.h>
#include "regex.h"

static auto_regex_code_t* _regex_api_create(const char* pattern, size_t size, size_t* errpos)
{
    int errcode;
    size_t tmp_errpos;
    PCRE2_SIZE error_offset;

    if (size == (size_t)-1)
    {
        size = PCRE2_ZERO_TERMINATED;
    }
    if (errpos == NULL)
    {
        errpos = &tmp_errpos;
    }

    /* Compile pattern */
    pcre2_code* code = pcre2_compile((PCRE2_SPTR)pattern, size, PCRE2_UTF,
        &errcode, &error_offset, NULL);
    if (code == NULL)
    {
        *errpos = error_offset;
        return NULL;
    }

    /* Try to enable jit support */
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

    *errpos = (size_t)-1;
    return (auto_regex_code_t*)code;
}

static void _regex_api_destroy(auto_regex_code_t* code)
{
    pcre2_code_free((pcre2_code*)code);
}

static size_t _regex_api_get_group_count(const auto_regex_code_t* code)
{
    pcre2_match_data* match_data = pcre2_match_data_create_from_pattern((pcre2_code*)code, NULL);
    size_t count = pcre2_get_ovector_count(match_data);
    pcre2_match_data_free(match_data);
    return count;
}

static int _regex_api_match(const auto_regex_code_t* self, const char* subject,
    size_t subject_len, size_t offset, auto_regex_cb cb, void* arg)
{
    pcre2_code* code = (pcre2_code*)self;
    pcre2_match_data* match_data = (cb != NULL) ?
        pcre2_match_data_create_from_pattern(code, NULL) : NULL;
    int ret = pcre2_match(code, (PCRE2_SPTR)subject, subject_len, offset, 0, match_data, NULL);
    if (ret <= 0)
    {
        ret = -1;
        goto finish;
    }

    if (cb != NULL)
    {
        PCRE2_SIZE* o_vector = pcre2_get_ovector_pointer(match_data);
        static_assert(sizeof(*o_vector) == sizeof(size_t), ERR_HINT_DEFINITION_MISMATCH);
        cb(subject, o_vector, ret, arg);
    }

finish:
    if (match_data != NULL)
    {
        pcre2_match_data_free(match_data);
    }
    return ret;
}

const auto_api_regex_t api_regex = {
    _regex_api_create,
    _regex_api_destroy,
    _regex_api_get_group_count,
    _regex_api_match,
};
