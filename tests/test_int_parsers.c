/*
 * test_int_parsers.c -- Integration tests for Vue, EJS, Nix, Gleam, HCL,
 *                       GraphQL, Julia, GDScript, Verse parsers and common
 *                       parser properties.
 *
 * Converted from legacy test_parsers.c to test_framework.h format.
 * Blade tests are in test_int_parser_blade.c (not duplicated here).
 */

#include "test_framework.h"
#include "test_helpers.h"

#include "parser_vue.h"
#include "parser_ejs.h"
#include "parser_nix.h"
#include "parser_gleam.h"
#include "parser_blade.h"
#include "parser_hcl.h"
#include "parser_graphql.h"
#include "parser_julia.h"
#include "parser_gdscript.h"
#include "parser_verse.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper: find a symbol by name in array. */
static const tt_symbol_t *find_by_name(const tt_symbol_t *syms, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (syms[i].name && strcmp(syms[i].name, name) == 0)
            return &syms[i];
    }
    return NULL;
}

/* Helper: find a symbol by qualified name in array. */
static const tt_symbol_t *find_by_qname(const tt_symbol_t *syms, int count, const char *qname)
{
    for (int i = 0; i < count; i++) {
        if (syms[i].qualified_name && strcmp(syms[i].qualified_name, qname) == 0)
            return &syms[i];
    }
    return NULL;
}

/* ---- Vue Parser Tests ---- */

TT_TEST(test_vue_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.vue" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_vue(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count > 0, "vue produces symbols");

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR(syms[i].language, "vue");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_vue_symbols)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.vue" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_vue(fixtures, files, 1, &syms, &count);

    /* Expected:
     * props (property, defineProps)
     * message (variable, ref)
     * count (variable, ref)
     * doubled (property, computed)
     * increment (function)
     * fetchData (function, arrow)
     */
    TT_ASSERT_GE_INT(count, 5);
    if (count < 5) { tt_symbol_array_free(syms, count); return; }

    const tt_symbol_t *s;

    s = find_by_name(syms, count, "props");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);
        TT_ASSERT_EQ_STR(s->signature, "defineProps");
    }

    s = find_by_name(syms, count, "message");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);
    }

    s = find_by_name(syms, count, "doubled");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);
    }

    s = find_by_name(syms, count, "increment");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);
    }

    s = find_by_name(syms, count, "fetchData");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_vue_line_numbers)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.vue" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_vue(fixtures, files, 1, &syms, &count);

    /* <script setup> is on line 5, content starts on line 6.
     * defineProps is on line 8 (first non-import line in script).
     * All lines should be > 5 (script starts after template). */
    for (int i = 0; i < count; i++) {
        TT_ASSERT_GT_INT(syms[i].line, 5);
    }

    tt_symbol_array_free(syms, count);
}

/* ---- EJS Parser Tests ---- */

TT_TEST(test_ejs_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.ejs" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_ejs(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count > 0, "ejs produces symbols");

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_INT(syms[i].kind, TT_KIND_DIRECTIVE);
        TT_ASSERT_EQ_STR(syms[i].language, "ejs");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ejs_includes)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.ejs" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_ejs(fixtures, files, 1, &syms, &count);

    /* Expected: header, sidebar, footer, header~2
     * The second 'partials/header' should get ~2 suffix. */
    TT_ASSERT_EQ_INT(count, 4);
    if (count != 4) { tt_symbol_array_free(syms, count); return; }

    const tt_symbol_t *s;
    s = find_by_qname(syms, count, "include.partials/header");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_STR(s->signature, "include('partials/header')");
    }

    s = find_by_qname(syms, count, "include.partials/sidebar");
    TT_ASSERT_NOT_NULL(s);

    s = find_by_qname(syms, count, "include.partials/footer");
    TT_ASSERT_NOT_NULL(s);

    /* Second occurrence of header should have ~2 suffix */
    s = find_by_qname(syms, count, "include.partials/header~2");
    TT_ASSERT_NOT_NULL(s);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ejs_keywords)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.ejs" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_ejs(fixtures, files, 1, &syms, &count);

    /* "partials/header" -> ["partials", "header"] */
    const tt_symbol_t *s = find_by_qname(syms, count, "include.partials/header");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"partials\"");
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"header\"");
    }

    tt_symbol_array_free(syms, count);
}

/* ---- Nix Parser Tests ---- */

TT_TEST(test_nix_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.nix" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_nix(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count > 0, "nix produces symbols");

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR(syms[i].language, "nix");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_nix_attributes)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.nix" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_nix(fixtures, files, 1, &syms, &count);

    /* Expected from sample.nix:
     * name = "my-project";          -> variable
     * version = "1.0";              -> variable
     * buildInputs = [...]           -> variable (starts with '[')
     * buildPhase = { stdenv }: ...  -> function (starts with {arg}:)
     * inherit (pkgs) gcc cmake;     -> 2 variables: gcc, cmake
     */
    TT_ASSERT_GE_INT(count, 5);
    if (count < 5) { tt_symbol_array_free(syms, count); return; }

    const tt_symbol_t *s;
    s = find_by_name(syms, count, "name");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);
    }

    s = find_by_name(syms, count, "buildInputs");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);
    }

    s = find_by_name(syms, count, "buildPhase");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);
    }

    s = find_by_name(syms, count, "gcc");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);
        TT_ASSERT_STR_CONTAINS(s->signature, "inherit (pkgs)");
    }

    s = find_by_name(syms, count, "cmake");
    TT_ASSERT_NOT_NULL(s);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_nix_skip_keywords)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    /* Nix keywords like "in", "then", "else" should be skipped. */
    const char *files[] = { "sample.nix" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_nix(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT(strcmp(syms[i].name, "in") != 0, "no 'in' keyword as symbol");
        TT_ASSERT(strcmp(syms[i].name, "then") != 0, "no 'then' keyword as symbol");
        TT_ASSERT(strcmp(syms[i].name, "else") != 0, "no 'else' keyword as symbol");
    }

    tt_symbol_array_free(syms, count);
}

/* ---- Gleam Parser Tests ---- */

TT_TEST(test_gleam_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.gleam" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_gleam(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count > 0, "gleam produces symbols");

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR(syms[i].language, "gleam");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_gleam_symbols)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.gleam" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_gleam(fixtures, files, 1, &syms, &count);

    /* Expected from sample.gleam:
     * import gleam/io       -> namespace
     * import gleam/string   -> namespace
     * pub fn main()         -> function
     * pub type User         -> type
     * pub const max_retries -> constant
     * fn helper(x)          -> function
     */
    TT_ASSERT_EQ_INT(count, 6);
    if (count != 6) { tt_symbol_array_free(syms, count); return; }

    const tt_symbol_t *s;

    s = find_by_name(syms, count, "gleam/io");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_NAMESPACE);
        TT_ASSERT_EQ_STR(s->signature, "import gleam/io");
    }

    s = find_by_name(syms, count, "main");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);
    }

    s = find_by_name(syms, count, "User");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_TYPE);
    }

    s = find_by_name(syms, count, "max_retries");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_CONSTANT);
    }

    s = find_by_name(syms, count, "helper");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_gleam_keywords)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = { "sample.gleam" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_gleam(fixtures, files, 1, &syms, &count);

    /* "gleam/io" -> split on / -> ["gleam", "io"] */
    const tt_symbol_t *s = find_by_name(syms, count, "gleam/io");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"gleam\"");
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"io\"");
    }

    /* "max_retries" -> split on _ -> ["max", "retries"] */
    s = find_by_name(syms, count, "max_retries");
    TT_ASSERT_NOT_NULL(s);
    if (s) {
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"max\"");
        TT_ASSERT_STR_CONTAINS(s->keywords, "\"retries\"");
    }

    tt_symbol_array_free(syms, count);
}

/* ---- Common properties tests ---- */

TT_TEST(test_common_content_hash)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    /* Verify that parsers produce non-empty content hashes. */
    const char *blade[] = { "sample.blade.php" };
    const char *gleam[] = { "sample.gleam" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_blade(fixtures, blade, 1, &syms, &count);
    for (int i = 0; i < count; i++) {
        TT_ASSERT_NOT_NULL(syms[i].content_hash);
        if (syms[i].content_hash) {
            TT_ASSERT_EQ_INT((int)strlen(syms[i].content_hash), 16);
        }
    }
    tt_symbol_array_free(syms, count);

    syms = NULL; count = 0;
    tt_parse_gleam(fixtures, gleam, 1, &syms, &count);
    for (int i = 0; i < count; i++) {
        TT_ASSERT_NOT_NULL(syms[i].content_hash);
        if (syms[i].content_hash) {
            TT_ASSERT_EQ_INT((int)strlen(syms[i].content_hash), 16);
        }
    }
    tt_symbol_array_free(syms, count);
}

TT_TEST(test_common_id_format)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    /* Verify symbol IDs follow the format: file::qualifiedName#kind */
    const char *files[] = { "sample.gleam" };
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_gleam(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_STR_CONTAINS(syms[i].id, "::");
        TT_ASSERT_STR_CONTAINS(syms[i].id, "#");
        TT_ASSERT_TRUE(tt_str_starts_with(syms[i].id, "sample.gleam::"));
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_empty_files)
{
    /* Passing no files should succeed with 0 symbols. */
    tt_symbol_t *syms = NULL;
    int count = 0;
    int rc;

    rc = tt_parse_blade("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);

    rc = tt_parse_vue("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);

    rc = tt_parse_ejs("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);

    rc = tt_parse_nix("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);

    rc = tt_parse_gleam("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);
}

/* ---- HCL/Terraform ---- */

TT_TEST(test_hcl_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_hcl.tf"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_hcl(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 5);

    const tt_symbol_t *resource = find_by_name(syms, count, "aws_instance.web");
    TT_ASSERT_NOT_NULL(resource);
    const tt_symbol_t *variable = find_by_name(syms, count, "region");
    TT_ASSERT_NOT_NULL(variable);
    const tt_symbol_t *module = find_by_name(syms, count, "vpc");
    TT_ASSERT_NOT_NULL(module);
    const tt_symbol_t *output = find_by_name(syms, count, "instance_ip");
    TT_ASSERT_NOT_NULL(output);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_hcl_all_block_types)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_hcl.tf"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_hcl(fixtures, files, 1, &syms, &count);

    /* resource "aws_instance" "web" -> CLASS */
    const tt_symbol_t *s = find_by_name(syms, count, "aws_instance.web");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* variable "region" -> VARIABLE */
    s = find_by_name(syms, count, "region");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    /* module "vpc" -> CLASS */
    s = find_by_name(syms, count, "vpc");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* output "instance_ip" -> PROPERTY */
    s = find_by_name(syms, count, "instance_ip");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);

    /* provider "aws" -> CLASS */
    s = find_by_name(syms, count, "aws");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* data "aws_ami" "ubuntu" -> PROPERTY */
    s = find_by_name(syms, count, "aws_ami.ubuntu");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);

    /* locals -> VARIABLE */
    s = find_by_name(syms, count, "locals");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    /* terraform -> CLASS */
    s = find_by_name(syms, count, "terraform");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* Total: 8 blocks in fixture */
    TT_ASSERT_EQ_INT(count, 8);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_hcl_signatures)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_hcl.tf"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_hcl(fixtures, files, 1, &syms, &count);

    const tt_symbol_t *s = find_by_name(syms, count, "aws_instance.web");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_STR_CONTAINS(s->signature, "resource");

    s = find_by_name(syms, count, "region");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_STR_CONTAINS(s->signature, "variable");

    tt_symbol_array_free(syms, count);
}

/* ---- GraphQL ---- */

TT_TEST(test_graphql_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_graphql.graphql"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_graphql(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 8);

    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "User"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "Node"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "Role"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "CreateUserInput"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "DateTime"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "GetUser"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "UserFields"));

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_graphql_all_constructs)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_graphql.graphql"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_graphql(fixtures, files, 1, &syms, &count);

    /* type User -> CLASS */
    const tt_symbol_t *s = find_by_name(syms, count, "User");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* interface Node -> INTERFACE */
    s = find_by_name(syms, count, "Node");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_INTERFACE);

    /* union SearchResult -> CLASS */
    s = find_by_name(syms, count, "SearchResult");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* enum Role -> ENUM */
    s = find_by_name(syms, count, "Role");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_ENUM);

    /* input CreateUserInput -> CLASS */
    s = find_by_name(syms, count, "CreateUserInput");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* scalar DateTime -> CLASS */
    s = find_by_name(syms, count, "DateTime");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* query GetUser -> FUNCTION */
    s = find_by_name(syms, count, "GetUser");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* mutation CreateUser -> FUNCTION */
    s = find_by_name(syms, count, "CreateUser");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* subscription OnUserCreated -> FUNCTION */
    s = find_by_name(syms, count, "OnUserCreated");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* fragment UserFields -> FUNCTION */
    s = find_by_name(syms, count, "UserFields");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* extend type User -> CLASS (second User entry) */
    /* Count User occurrences -- should be 2 (original + extend) */
    int user_count = 0;
    for (int i = 0; i < count; i++) {
        if (syms[i].name && strcmp(syms[i].name, "User") == 0)
            user_count++;
    }
    TT_ASSERT_EQ_INT(user_count, 2);

    tt_symbol_array_free(syms, count);
}

/* ---- Julia ---- */

TT_TEST(test_julia_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_julia.jl"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_julia(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 6);

    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "MyModule"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "Point"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "Circle"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "area"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "assert_positive"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "MAX_RADIUS"));

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_julia_all_constructs)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_julia.jl"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_julia(fixtures, files, 1, &syms, &count);

    /* module MyModule -> CLASS */
    const tt_symbol_t *s = find_by_name(syms, count, "MyModule");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* abstract type AbstractShape -> CLASS */
    s = find_by_name(syms, count, "AbstractShape");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* struct Point -> CLASS */
    s = find_by_name(syms, count, "Point");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* mutable struct Circle -> CLASS */
    s = find_by_name(syms, count, "Circle");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* function area -> FUNCTION */
    s = find_by_name(syms, count, "area");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* function perimeter -> FUNCTION */
    s = find_by_name(syms, count, "perimeter");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* macro assert_positive -> FUNCTION */
    s = find_by_name(syms, count, "assert_positive");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* const MAX_RADIUS -> CONSTANT */
    s = find_by_name(syms, count, "MAX_RADIUS");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CONSTANT);

    /* const MIN_RADIUS -> CONSTANT */
    s = find_by_name(syms, count, "MIN_RADIUS");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CONSTANT);

    /* Total: module + abstract type + 2 structs + 2 functions + 1 macro + 2 consts = 9 */
    TT_ASSERT_EQ_INT(count, 9);

    tt_symbol_array_free(syms, count);
}

/* ---- GDScript ---- */

TT_TEST(test_gdscript_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_gdscript.gd"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_gdscript(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 6);

    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "Player"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "health_changed"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "State"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "MAX_HEALTH"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "_ready"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "take_damage"));

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_gdscript_all_constructs)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_gdscript.gd"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_gdscript(fixtures, files, 1, &syms, &count);

    /* class_name Player -> CLASS */
    const tt_symbol_t *s = find_by_name(syms, count, "Player");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* signal health_changed -> PROPERTY */
    s = find_by_name(syms, count, "health_changed");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);

    /* signal died -> PROPERTY */
    s = find_by_name(syms, count, "died");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_PROPERTY);

    /* enum State -> ENUM */
    s = find_by_name(syms, count, "State");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_ENUM);

    /* const MAX_HEALTH -> CONSTANT */
    s = find_by_name(syms, count, "MAX_HEALTH");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CONSTANT);

    /* const SPEED: float -> CONSTANT (typed const) */
    s = find_by_name(syms, count, "SPEED");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CONSTANT);

    /* @export var health -> VARIABLE */
    s = find_by_name(syms, count, "health");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    /* @export var armor -> VARIABLE */
    s = find_by_name(syms, count, "armor");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    /* class InnerStats -> CLASS (inner class) */
    s = find_by_name(syms, count, "InnerStats");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* func _ready -> FUNCTION */
    s = find_by_name(syms, count, "_ready");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* func take_damage -> FUNCTION */
    s = find_by_name(syms, count, "take_damage");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* func heal -> FUNCTION */
    s = find_by_name(syms, count, "heal");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* Total: 1 class_name + 2 signals + 1 enum + 2 consts + 2 exports + 1 inner class + 3 funcs = 12 */
    TT_ASSERT_EQ_INT(count, 12);

    tt_symbol_array_free(syms, count);
}

/* ---- Verse ---- */

TT_TEST(test_verse_basic)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_verse.verse"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_verse(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 3);

    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "player_manager"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "score_tracker"));

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_verse_all_constructs)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_verse.verse"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_verse(fixtures, files, 1, &syms, &count);

    /* player_manager := class -> CLASS */
    const tt_symbol_t *s = find_by_name(syms, count, "player_manager");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* score_tracker := class -> CLASS */
    s = find_by_name(syms, count, "score_tracker");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_CLASS);

    /* OnBegin<override>()<decides><transacts> : void = -> FUNCTION */
    s = find_by_name(syms, count, "OnBegin");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* AddPlayer(Player : player) : void = -> FUNCTION */
    s = find_by_name(syms, count, "AddPlayer");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* RemovePlayer(Player : player) : void = -> FUNCTION */
    s = find_by_name(syms, count, "RemovePlayer");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* GetScore(Player : player) : int = -> FUNCTION */
    s = find_by_name(syms, count, "GetScore");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_FUNCTION);

    /* game_config : game_config_struct = -> VARIABLE (top-level) */
    s = find_by_name(syms, count, "game_config");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    /* max_players : int = 16 -> VARIABLE (top-level) */
    s = find_by_name(syms, count, "max_players");
    TT_ASSERT_NOT_NULL(s);
    if (s) TT_ASSERT_EQ_INT(s->kind, TT_KIND_VARIABLE);

    tt_symbol_array_free(syms, count);
}

/* ---- Vue Composition API ---- */

TT_TEST(test_vue_composition_api)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_vue_composition.vue"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_vue(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(count >= 8);

    /* defineProps / defineEmits / defineExpose */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "props"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "emits"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "expose"));

    /* ref/computed */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "message"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "doubled"));

    /* watch / watchEffect */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "watch"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "watchEffect"));

    /* lifecycle hooks */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "onMounted"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "onUnmounted"));

    /* provide / inject */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "provide"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "inject"));

    /* functions */
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "increment"));
    TT_ASSERT_NOT_NULL(find_by_name(syms, count, "fetchData"));

    tt_symbol_array_free(syms, count);
}

/* ---- Public runner ---- */

void run_int_parsers_tests(void)
{
    TT_SUITE("Integration: Vue Parser");
    TT_RUN(test_vue_basic);
    TT_RUN(test_vue_symbols);
    TT_RUN(test_vue_line_numbers);

    TT_SUITE("Integration: EJS Parser");
    TT_RUN(test_ejs_basic);
    TT_RUN(test_ejs_includes);
    TT_RUN(test_ejs_keywords);

    TT_SUITE("Integration: Nix Parser");
    TT_RUN(test_nix_basic);
    TT_RUN(test_nix_attributes);
    TT_RUN(test_nix_skip_keywords);

    TT_SUITE("Integration: Gleam Parser");
    TT_RUN(test_gleam_basic);
    TT_RUN(test_gleam_symbols);
    TT_RUN(test_gleam_keywords);

    TT_SUITE("Integration: HCL Parser");
    TT_RUN(test_hcl_basic);
    TT_RUN(test_hcl_all_block_types);
    TT_RUN(test_hcl_signatures);

    TT_SUITE("Integration: GraphQL Parser");
    TT_RUN(test_graphql_basic);
    TT_RUN(test_graphql_all_constructs);

    TT_SUITE("Integration: Julia Parser");
    TT_RUN(test_julia_basic);
    TT_RUN(test_julia_all_constructs);

    TT_SUITE("Integration: GDScript Parser");
    TT_RUN(test_gdscript_basic);
    TT_RUN(test_gdscript_all_constructs);

    TT_SUITE("Integration: Verse Parser");
    TT_RUN(test_verse_basic);
    TT_RUN(test_verse_all_constructs);

    TT_SUITE("Integration: Vue Composition API");
    TT_RUN(test_vue_composition_api);

    TT_SUITE("Integration: Common Parser Properties");
    TT_RUN(test_common_content_hash);
    TT_RUN(test_common_id_format);
    TT_RUN(test_empty_files);
}
