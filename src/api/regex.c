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

static auto_regex_code_t* _regex_api_create(const char* pattern, size_t size)
{
    int errcode;
    PCRE2_SIZE error_offset;

    if (size == (size_t)-1)
    {
        size = PCRE2_ZERO_TERMINATED;
    }

    /* Compile pattern */
    pcre2_code* code = pcre2_compile((PCRE2_SPTR)pattern, size, PCRE2_UTF,
        &errcode, &error_offset, NULL);
    if (code == NULL)
    {
        return NULL;
    }

    /* Try to enable jit support */
    pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

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

static int _regex_api_match(const auto_regex_code_t* code, const char* subject,
    size_t subject_len, size_t* groups, size_t group_len)
{
    pcre2_match_data* match_data = NULL;
    if (groups != NULL && group_len > 0)
    {
        match_data = pcre2_match_data_create_from_pattern((pcre2_code*)code, NULL);
    }

    if (subject_len == (size_t)-1)
    {
        subject_len = PCRE2_ZERO_TERMINATED;
    }

    int ret = pcre2_match((pcre2_code*)code, (PCRE2_SPTR)subject, subject_len,
        0, 0, match_data, NULL);
    if (ret <= 0)
    {
        ret = -1;
        goto finish;
    }

    PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
    size_t ovector_len = pcre2_get_ovector_count(match_data);
    size_t copy_size = ovector_len < group_len ? ovector_len : group_len;

    size_t i;
    for (i = 0; i < copy_size; i++)
    {
        groups[2 * i] = ovector[2 * i];
        groups[2 * i + 1] = ovector[2 * i + 1];
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