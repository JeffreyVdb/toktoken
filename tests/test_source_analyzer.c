/*
 * test_source_analyzer.c -- Unit tests for source_analyzer module.
 */

#include "test_framework.h"
#include "source_analyzer.h"
#include "symbol_kind.h"

#include <stdlib.h>
#include <string.h>

/* ---- estimateEndLine tests ---- */

TT_TEST(test_sa_end_line_simple_function)
{
    const char *lines[] = {
        "function foo() {",
        "    return 1;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 3, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(3, end);
}

TT_TEST(test_sa_end_line_nested_braces)
{
    const char *lines[] = {
        "function nested() {",
        "    if (true) {",
        "        foreach ($x as $y) {",
        "            echo $y;",
        "        }",
        "    }",
        "}",
    };
    int end = tt_estimate_end_line(lines, 7, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(7, end);
}

TT_TEST(test_sa_end_line_skips_string_braces)
{
    const char *lines[] = {
        "function braceStr() {",
        "    $x = \"has { and }\";",
        "    return $x;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 4, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(4, end);
}

TT_TEST(test_sa_end_line_skips_single_quoted_string_braces)
{
    const char *lines[] = {
        "function braceStr() {",
        "    $x = 'has { and }';",
        "    return $x;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 4, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(4, end);
}

TT_TEST(test_sa_end_line_skips_line_comment_braces)
{
    const char *lines[] = {
        "function foo() {",
        "    // this { is a comment",
        "    $x = 1;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 4, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(4, end);
}

TT_TEST(test_sa_end_line_skips_block_comment_braces)
{
    const char *lines[] = {
        "function foo() {",
        "    /* block { comment",
        "       still } here */",
        "    $x = 1;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 5, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(5, end);
}

TT_TEST(test_sa_end_line_skips_hash_comment)
{
    const char *lines[] = {
        "function foo() {",
        "    # comment with { brace",
        "    $x = 1;",
        "}",
    };
    int end = tt_estimate_end_line(lines, 4, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(4, end);
}

TT_TEST(test_sa_end_line_single_line_kind)
{
    const char *lines[] = {
        "const FOO = 1;",
    };
    int end = tt_estimate_end_line(lines, 1, 1, TT_KIND_CONSTANT);
    TT_ASSERT_EQ_INT(1, end);
}

TT_TEST(test_sa_end_line_property)
{
    const char *lines[] = {
        "private string $name;",
    };
    int end = tt_estimate_end_line(lines, 1, 1, TT_KIND_PROPERTY);
    TT_ASSERT_EQ_INT(1, end);
}

TT_TEST(test_sa_end_line_fallback_no_braces)
{
    const char *lines[100];
    char line_bufs[100][32];
    for (int i = 0; i < 100; i++) {
        snprintf(line_bufs[i], sizeof(line_bufs[i]), "    line %d", i);
        lines[i] = line_bufs[i];
    }
    int end = tt_estimate_end_line(lines, 100, 1, TT_KIND_FUNCTION);
    TT_ASSERT_EQ_INT(51, end); /* start + 50 */
}

/* ---- extractDocstring tests ---- */

TT_TEST(test_sa_docstring_phpdoc)
{
    const char *lines[] = {
        "<?php",
        "/**",
        " * This is a docstring.",
        " * @param int $x",
        " */",
        "function foo() {}",
    };
    char *doc = tt_extract_docstring(lines, 6, 6);
    TT_ASSERT_NOT_NULL(doc);
    TT_ASSERT_STR_CONTAINS(doc, "This is a docstring.");
    free(doc);
}

TT_TEST(test_sa_docstring_line_comments)
{
    const char *lines[] = {
        "<?php",
        "// This is a line comment doc.",
        "// Second line.",
        "function bar() {}",
    };
    char *doc = tt_extract_docstring(lines, 4, 4);
    TT_ASSERT_NOT_NULL(doc);
    TT_ASSERT_STR_CONTAINS(doc, "This is a line comment doc.");
    TT_ASSERT_STR_CONTAINS(doc, "Second line.");
    free(doc);
}

TT_TEST(test_sa_docstring_hash_comments)
{
    const char *lines[] = {
        "<?php",
        "# Hash comment docstring.",
        "function baz() {}",
    };
    char *doc = tt_extract_docstring(lines, 3, 3);
    TT_ASSERT_NOT_NULL(doc);
    TT_ASSERT_STR_CONTAINS(doc, "Hash comment docstring.");
    free(doc);
}

TT_TEST(test_sa_docstring_no_docstring)
{
    const char *lines[] = {
        "<?php",
        "",
        "function noDoc() {}",
    };
    char *doc = tt_extract_docstring(lines, 3, 3);
    TT_ASSERT_NOT_NULL(doc);
    TT_ASSERT_EQ_STR("", doc);
    free(doc);
}

TT_TEST(test_sa_docstring_first_line)
{
    const char *lines[] = {
        "function first() {}",
    };
    char *doc = tt_extract_docstring(lines, 1, 1);
    TT_ASSERT_NOT_NULL(doc);
    TT_ASSERT_EQ_STR("", doc);
    free(doc);
}

/* ---- extractKeywords tests ---- */

TT_TEST(test_sa_keywords_camelcase)
{
    char *kw = tt_extract_keywords("UserService.getUserData");
    TT_ASSERT_NOT_NULL(kw);
    TT_ASSERT_STR_CONTAINS(kw, "user");
    TT_ASSERT_STR_CONTAINS(kw, "service");
    TT_ASSERT_STR_CONTAINS(kw, "get");
    TT_ASSERT_STR_CONTAINS(kw, "data");
    free(kw);
}

TT_TEST(test_sa_keywords_snakecase)
{
    char *kw = tt_extract_keywords("get_user_data");
    TT_ASSERT_NOT_NULL(kw);
    TT_ASSERT_STR_CONTAINS(kw, "get");
    TT_ASSERT_STR_CONTAINS(kw, "user");
    TT_ASSERT_STR_CONTAINS(kw, "data");
    free(kw);
}

TT_TEST(test_sa_keywords_filters_single_char)
{
    char *kw = tt_extract_keywords("X.a");
    TT_ASSERT_NOT_NULL(kw);
    /* Single-char words should be filtered. The JSON may be [] or contain multi-char only. */
    TT_ASSERT_STR_NOT_CONTAINS(kw, "\"a\"");
    free(kw);
}

void run_source_analyzer_tests(void)
{
    TT_RUN(test_sa_end_line_simple_function);
    TT_RUN(test_sa_end_line_nested_braces);
    TT_RUN(test_sa_end_line_skips_string_braces);
    TT_RUN(test_sa_end_line_skips_single_quoted_string_braces);
    TT_RUN(test_sa_end_line_skips_line_comment_braces);
    TT_RUN(test_sa_end_line_skips_block_comment_braces);
    TT_RUN(test_sa_end_line_skips_hash_comment);
    TT_RUN(test_sa_end_line_single_line_kind);
    TT_RUN(test_sa_end_line_property);
    TT_RUN(test_sa_end_line_fallback_no_braces);
    TT_RUN(test_sa_docstring_phpdoc);
    TT_RUN(test_sa_docstring_line_comments);
    TT_RUN(test_sa_docstring_hash_comments);
    TT_RUN(test_sa_docstring_no_docstring);
    TT_RUN(test_sa_docstring_first_line);
    TT_RUN(test_sa_keywords_camelcase);
    TT_RUN(test_sa_keywords_snakecase);
    TT_RUN(test_sa_keywords_filters_single_char);
}
