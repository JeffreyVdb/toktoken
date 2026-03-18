/*
 * test_jinja_strip.c -- Unit tests for jinja_strip module.
 */

#include "test_framework.h"
#include "jinja_strip.h"

#include <string.h>
#include <stdlib.h>

TT_TEST(test_jinja_strip_expression)
{
    const char *input = "SELECT {{ ref('model') }} FROM table";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    TT_ASSERT_NOT_NULL(result);
    if (!result) return;

    /* Jinja expression replaced with spaces */
    TT_ASSERT(strstr(result, "{{") == NULL, "should not contain {{");
    TT_ASSERT(strstr(result, "}}") == NULL, "should not contain }}");
    TT_ASSERT(strstr(result, "SELECT") != NULL, "should keep SELECT");
    TT_ASSERT(strstr(result, "FROM table") != NULL, "should keep FROM table");
    TT_ASSERT_EQ_INT((int)out_len, (int)strlen(input));
    free(result);
}

TT_TEST(test_jinja_strip_block)
{
    const char *input = "{% if condition %}\nSELECT 1\n{% endif %}";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    TT_ASSERT_NOT_NULL(result);
    if (!result) return;

    TT_ASSERT(strstr(result, "{%") == NULL, "should not contain {%");
    TT_ASSERT(strstr(result, "SELECT 1") != NULL, "should keep SQL");

    /* Newlines preserved */
    int newlines = 0;
    for (size_t i = 0; i < out_len; i++)
        if (result[i] == '\n') newlines++;
    TT_ASSERT_EQ_INT(newlines, 2);

    free(result);
}

TT_TEST(test_jinja_strip_comment)
{
    const char *input = "SELECT 1 {# this is a comment #} FROM dual";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    TT_ASSERT_NOT_NULL(result);
    if (!result) return;

    TT_ASSERT(strstr(result, "{#") == NULL, "should not contain {#");
    TT_ASSERT(strstr(result, "#}") == NULL, "should not contain #}");
    TT_ASSERT(strstr(result, "SELECT 1") != NULL, "should keep SQL");
    free(result);
}

TT_TEST(test_jinja_strip_multiline)
{
    const char *input =
        "{% macro test(arg) %}\n"
        "  SELECT {{ arg }}\n"
        "  FROM {{ ref('table') }}\n"
        "{% endmacro %}";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    TT_ASSERT_NOT_NULL(result);
    if (!result) return;

    /* Newlines preserved */
    int newlines = 0;
    for (size_t i = 0; i < out_len; i++)
        if (result[i] == '\n') newlines++;
    TT_ASSERT_EQ_INT(newlines, 3);

    TT_ASSERT(strstr(result, "SELECT") != NULL, "should keep SELECT");
    TT_ASSERT(strstr(result, "FROM") != NULL, "should keep FROM");
    free(result);
}

TT_TEST(test_jinja_strip_no_jinja)
{
    const char *input = "SELECT id, name FROM users WHERE active = 1";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    TT_ASSERT_NULL(result);
    TT_ASSERT_EQ_INT((int)out_len, (int)strlen(input));
}

TT_TEST(test_jinja_strip_empty)
{
    size_t out_len = 99;
    char *result = tt_jinja_strip("", 0, &out_len);
    TT_ASSERT_NULL(result);
    TT_ASSERT_EQ_INT((int)out_len, 0);
}

TT_TEST(test_jinja_strip_null)
{
    size_t out_len = 99;
    char *result = tt_jinja_strip(NULL, 0, &out_len);
    TT_ASSERT_NULL(result);
    TT_ASSERT_EQ_INT((int)out_len, 0);
}

TT_TEST(test_jinja_strip_unclosed_delimiter)
{
    const char *input = "SELECT {{ ref('model')";
    size_t out_len = 0;
    char *result = tt_jinja_strip(input, strlen(input), &out_len);
    /* Unclosed delimiter: content returned but unclosed block left as-is */
    if (result) {
        TT_ASSERT(strstr(result, "SELECT") != NULL, "should keep SQL");
        free(result);
    }
}

void run_jinja_strip_tests(void)
{
    TT_RUN(test_jinja_strip_expression);
    TT_RUN(test_jinja_strip_block);
    TT_RUN(test_jinja_strip_comment);
    TT_RUN(test_jinja_strip_multiline);
    TT_RUN(test_jinja_strip_no_jinja);
    TT_RUN(test_jinja_strip_empty);
    TT_RUN(test_jinja_strip_null);
    TT_RUN(test_jinja_strip_unclosed_delimiter);
}
