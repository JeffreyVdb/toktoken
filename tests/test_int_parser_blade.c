/*
 * test_int_parser_blade.c -- Integration tests for parser_blade module.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "parser_blade.h"
#include "symbol.h"
#include "symbol_kind.h"

#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int sym_has_name(const tt_symbol_t *syms, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (syms[i].name && strcmp(syms[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static const tt_symbol_t *sym_find_qname(const tt_symbol_t *syms, int count,
                                          const char *qname)
{
    for (int i = 0; i < count; i++) {
        if (syms[i].qualified_name && strcmp(syms[i].qualified_name, qname) == 0)
            return &syms[i];
    }
    return NULL;
}

TT_TEST(test_int_blade_extracts_directives)
{
    const char *fixture = tt_test_fixtures_dir();
    if (!fixture) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/blade-project", fixture);

    const char *files[] = {"resources/views/sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_blade(root, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT(count > 0, "should extract directives");

    TT_ASSERT(sym_has_name(syms, count, "layouts.app"), "should find layouts.app");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_kind_is_directive)
{
    const char *fixture = tt_test_fixtures_dir();
    if (!fixture) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/blade-project", fixture);

    const char *files[] = {"resources/views/sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(root, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("directive", tt_kind_to_str(syms[i].kind));
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_language_is_blade)
{
    const char *fixture = tt_test_fixtures_dir();
    if (!fixture) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/blade-project", fixture);

    const char *files[] = {"resources/views/sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(root, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("blade", syms[i].language);
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_id_format)
{
    const char *fixture = tt_test_fixtures_dir();
    if (!fixture) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/blade-project", fixture);

    const char *files[] = {"resources/views/sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(root, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT(strncmp(syms[i].id, "resources/views/sample.blade.php::", 33) == 0,
                   "ID should start with file path");
        TT_ASSERT(strstr(syms[i].id, "#directive") != NULL,
                   "ID should end with #directive");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_empty_file_list)
{
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_blade("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(0, count);
}

/* ---- Legacy blade tests using tests/fixtures/sample.blade.php ---- */

/* Helper: get project root for fixture-based tests */
static const char *blade_fixture_root(void)
{
    const char *f = tt_test_fixtures_dir();
    return f;  /* NULL if not found */
}

TT_TEST(test_int_blade_directives_detail)
{
    const char *fixtures = blade_fixture_root();
    if (!fixtures) return;

    const char *files[] = {"sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_blade(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 8);
    if (count != 8) { tt_symbol_array_free(syms, count); return; }

    const tt_symbol_t *s = sym_find_qname(syms, count, "extends.layouts.app");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_STR(s->name, "layouts.app");
        TT_ASSERT_EQ_INT(s->line, 1);
    }

    TT_ASSERT_NOT_NULL(sym_find_qname(syms, count, "section.title"));
    TT_ASSERT_NOT_NULL(sym_find_qname(syms, count, "include.partials.header"));
    TT_ASSERT_NOT_NULL(sym_find_qname(syms, count, "yield.sidebar"));

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_no_dedup_suffix)
{
    const char *fixtures = blade_fixture_root();
    if (!fixtures) return;

    const char *files[] = {"sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_STR_NOT_CONTAINS(syms[i].qualified_name, "~");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_keywords)
{
    const char *fixtures = blade_fixture_root();
    if (!fixtures) return;

    const char *files[] = {"sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(fixtures, files, 1, &syms, &count);

    const tt_symbol_t *s = sym_find_qname(syms, count, "extends.layouts.app");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"extends\"");
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"layouts\"");
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"app\"");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_signature)
{
    const char *fixtures = blade_fixture_root();
    if (!fixtures) return;

    const char *files[] = {"sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(fixtures, files, 1, &syms, &count);

    const tt_symbol_t *s = sym_find_qname(syms, count, "extends.layouts.app");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_STR(s->signature, "@extends('layouts.app')");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_int_blade_normalize_directive)
{
    const char *fixtures = blade_fixture_root();
    if (!fixtures) return;

    const char *files[] = {"sample.blade.php"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(fixtures, files, 1, &syms, &count);
    if (count == 0) return;

    for (int i = 0; i < count; i++) {
        TT_ASSERT_FALSE(tt_str_starts_with(syms[i].qualified_name, "includeIf."));
        TT_ASSERT_FALSE(tt_str_starts_with(syms[i].qualified_name, "includeWhen."));
        TT_ASSERT_FALSE(tt_str_starts_with(syms[i].qualified_name, "includeFirst."));
        TT_ASSERT_FALSE(tt_str_starts_with(syms[i].qualified_name, "prepend."));
    }

    tt_symbol_array_free(syms, count);
}

void run_int_parser_blade_tests(void)
{
    TT_RUN(test_int_blade_extracts_directives);
    TT_RUN(test_int_blade_kind_is_directive);
    TT_RUN(test_int_blade_language_is_blade);
    TT_RUN(test_int_blade_id_format);
    TT_RUN(test_int_blade_empty_file_list);
    TT_RUN(test_int_blade_directives_detail);
    TT_RUN(test_int_blade_no_dedup_suffix);
    TT_RUN(test_int_blade_keywords);
    TT_RUN(test_int_blade_signature);
    TT_RUN(test_int_blade_normalize_directive);
}
