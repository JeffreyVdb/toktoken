/*
 * test_int_parser_extra.c -- Integration tests for Assembly, AutoHotkey,
 *                            OpenAPI, and XML parsers.
 *
 * These parsers had ZERO test coverage. Tests use real-world representative
 * fixtures covering multiple dialects, edge cases, and malformed input.
 */

#include "test_framework.h"
#include "test_helpers.h"

#include "parser_asm.h"
#include "parser_autohotkey.h"
#include "parser_openapi.h"
#include "parser_xml.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>

/* ---- Helpers ---- */

static const tt_symbol_t *find_sym(const tt_symbol_t *syms, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (syms[i].name && strcmp(syms[i].name, name) == 0)
            return &syms[i];
    }
    return NULL;
}

static int count_kind(const tt_symbol_t *syms, int count, tt_symbol_kind_e kind)
{
    int n = 0;
    for (int i = 0; i < count; i++) {
        if (syms[i].kind == kind) n++;
    }
    return n;
}

static int has_sym(const tt_symbol_t *syms, int count, const char *name)
{
    return find_sym(syms, count, name) != NULL;
}

/* ==========================================================================
 * ASSEMBLY PARSER -- WLA-DX Dialect
 * ========================================================================== */

TT_TEST(test_asm_wladx_labels)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* Global labels inside sections */
    TT_ASSERT(has_sym(syms, count, "core.object.init"), "should find core.object.init label");
    TT_ASSERT(has_sym(syms, count, "core.object.create"), "should find core.object.create label");
    TT_ASSERT(has_sym(syms, count, "scummvm.executeOpcode"), "should find scummvm.executeOpcode");

    /* Labels should be FUNCTION kind */
    const tt_symbol_t *s = find_sym(syms, count, "core.object.init");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_local_labels_excluded)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* _fetchByte is a local label (_prefix), should be excluded */
    TT_ASSERT(!has_sym(syms, count, "_fetchByte"), "_prefixed local labels should be excluded");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_sections)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .section "name" -> CLASS */
    TT_ASSERT(has_sym(syms, count, "oophandler"), "should find oophandler section");
    TT_ASSERT(has_sym(syms, count, "ScummVM dispatch"), "should find ScummVM dispatch section");

    const tt_symbol_t *s = find_sym(syms, count, "oophandler");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_ramsection)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .ramsection "global vars" -> CLASS */
    TT_ASSERT(has_sym(syms, count, "global vars"), "should find ramsection");
    const tt_symbol_t *s = find_sym(syms, count, "global vars");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_macros)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .macro NAME -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "CLASS"), "should find CLASS macro");
    TT_ASSERT(has_sym(syms, count, "METHOD"), "should find METHOD macro");
    TT_ASSERT(has_sym(syms, count, "SCRIPT"), "should find SCRIPT macro");

    const tt_symbol_t *s = find_sym(syms, count, "CLASS");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_constants)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .define NAME value -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "maxNumberOopObjs"), "should find .define constant");
    TT_ASSERT(has_sym(syms, count, "oopStackTst"), "should find .define constant");
    /* .def shorthand */
    TT_ASSERT(has_sym(syms, count, "oopStackObj.length"), "should find .def constant");

    const tt_symbol_t *s = find_sym(syms, count, "maxNumberOopObjs");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);
        /* Signature should include the value */
        TT_ASSERT_STR_CONTAINS(s->signature, "48");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_struct)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .STRUCT name -> TYPE */
    TT_ASSERT(has_sym(syms, count, "oopStackObj"), "should find struct");
    const tt_symbol_t *s = find_sym(syms, count, "oopStackObj");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_TYPE, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_enum_members)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .ENUM body members -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "OBJR_noErr"), "should find enum member OBJR_noErr");
    TT_ASSERT(has_sym(syms, count, "OBJR_kill"), "should find enum member OBJR_kill");

    const tt_symbol_t *s = find_sym(syms, count, "OBJR_noErr");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_docstrings)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* Preceding ;comments should be captured as docstrings */
    const tt_symbol_t *s = find_sym(syms, count, "core.object.init");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "clear oop stack");
    }

    s = find_sym(syms, count, "core.object.create");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "number of object to create");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_wladx_language_field)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_wladx.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("asm", syms[i].language);
    }

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * ASSEMBLY PARSER -- NASM Dialect
 * ========================================================================== */

TT_TEST(test_asm_nasm_labels)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* print_hello is a global label */
    TT_ASSERT(has_sym(syms, count, "print_hello"), "should find print_hello label");
    const tt_symbol_t *s = find_sym(syms, count, "print_hello");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_nasm_defines)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* %define NAME value -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "BUFFER_SIZE"), "should find %define BUFFER_SIZE");
    TT_ASSERT(has_sym(syms, count, "MAX_RETRIES"), "should find %define MAX_RETRIES");

    const tt_symbol_t *s = find_sym(syms, count, "BUFFER_SIZE");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_nasm_macros)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* %macro NAME -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "PROLOGUE"), "should find PROLOGUE macro");
    TT_ASSERT(has_sym(syms, count, "EPILOGUE"), "should find EPILOGUE macro");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_nasm_equ_constants)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* NAME equ VALUE -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "len"), "should find equ constant 'len'");
    const tt_symbol_t *s = find_sym(syms, count, "len");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_nasm_sections)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* NASM section .text/.data/.bss -- toktoken doesn't emit symbols for
     * bare .text/.data/.bss (too noisy), but sets them as current_section.
     * Verify the struct is detected instead. */
    TT_ASSERT(has_sym(syms, count, "point"), "should find struc 'point'");
    const tt_symbol_t *s = find_sym(syms, count, "point");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_TYPE, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_nasm_docstring)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_nasm.asm"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* The comment "Print a hello message to stdout" precedes print_hello */
    const tt_symbol_t *s = find_sym(syms, count, "print_hello");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "Print a hello message");
    }

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * ASSEMBLY PARSER -- GAS (GNU Assembler) Dialect
 * ========================================================================== */

TT_TEST(test_asm_gas_labels)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_gas.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* GAS labels */
    TT_ASSERT(has_sym(syms, count, "main"), "should find main label");
    TT_ASSERT(has_sym(syms, count, "helper"), "should find helper label");
    /* message: is in .data section -- also a label */
    TT_ASSERT(has_sym(syms, count, "message"), "should find message label");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_gas_constants)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_gas.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .set NAME, value -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "MAX_COUNT"), "should find .set MAX_COUNT");
    /* .equ NAME, value -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "STACK_SIZE"), "should find .equ STACK_SIZE");

    const tt_symbol_t *s = find_sym(syms, count, "MAX_COUNT");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "256");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_gas_macros)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_gas.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .macro NAME -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "save_regs"), "should find save_regs macro");
    TT_ASSERT(has_sym(syms, count, "restore_regs"), "should find restore_regs macro");

    const tt_symbol_t *s = find_sym(syms, count, "save_regs");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_gas_block_comments)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_gas.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* The block comment at the top should not generate symbols
     * and should not confuse the parser */
    TT_ASSERT(count > 0, "parser should survive block comments");

    /* Verify no spurious symbols from inside the block comment */
    TT_ASSERT(!has_sym(syms, count, "GAS"), "block comment content should not be a symbol");
    TT_ASSERT(!has_sym(syms, count, "Demonstrates"), "block comment content should not be a symbol");

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * ASSEMBLY PARSER -- CA65 Dialect
 * ========================================================================== */

TT_TEST(test_asm_ca65_procs)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_ca65.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* .proc NAME -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "main"), "should find main proc");
    TT_ASSERT(has_sym(syms, count, "irq_handler"), "should find irq_handler proc");
    TT_ASSERT(has_sym(syms, count, "nmi_handler"), "should find nmi_handler proc");

    const tt_symbol_t *s = find_sym(syms, count, "irq_handler");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, ".proc");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_ca65_segments)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_ca65.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .segment "NAME" -> CLASS */
    TT_ASSERT(has_sym(syms, count, "CODE"), "should find CODE segment");
    TT_ASSERT(has_sym(syms, count, "VECTORS"), "should find VECTORS segment");
    TT_ASSERT(has_sym(syms, count, "CHARS"), "should find CHARS segment");

    const tt_symbol_t *s = find_sym(syms, count, "CODE");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_ca65_constants)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_ca65.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .define NAME value -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "SCREEN_WIDTH"), "should find .define SCREEN_WIDTH");
    TT_ASSERT(has_sym(syms, count, "SCREEN_HEIGHT"), "should find .define SCREEN_HEIGHT");

    /* NAME = VALUE -> CONSTANT (CA65 style) */
    TT_ASSERT(has_sym(syms, count, "PPU_CTRL"), "should find PPU_CTRL constant");
    TT_ASSERT(has_sym(syms, count, "PPU_MASK"), "should find PPU_MASK constant");
    TT_ASSERT(has_sym(syms, count, "PPU_STATUS"), "should find PPU_STATUS constant");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_ca65_macros)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_ca65.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* .macro NAME / .endmacro -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "set_ppu_addr"), "should find set_ppu_addr macro");
    TT_ASSERT(has_sym(syms, count, "wait_vblank"), "should find wait_vblank macro");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_asm_ca65_docstrings)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_asm_ca65.s"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_asm(fixtures, files, 1, &syms, &count);

    /* ; IRQ handler -- services hardware interrupts */
    const tt_symbol_t *s = find_sym(syms, count, "irq_handler");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "IRQ handler");
    }

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * ASSEMBLY PARSER -- Edge Cases
 * ========================================================================== */

TT_TEST(test_asm_empty_file)
{
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(0, count);
}

TT_TEST(test_asm_multi_file)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    /* Parse all 4 dialect files in a single call */
    const char *files[] = {
        "sample_asm_wladx.asm",
        "sample_asm_nasm.asm",
        "sample_asm_gas.s",
        "sample_asm_ca65.s"
    };
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_asm(fixtures, files, 4, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* Should find symbols from all 4 files */
    TT_ASSERT(has_sym(syms, count, "core.object.init"), "WLA-DX symbol");
    TT_ASSERT(has_sym(syms, count, "PROLOGUE"), "NASM symbol");
    TT_ASSERT(has_sym(syms, count, "save_regs"), "GAS symbol");
    TT_ASSERT(has_sym(syms, count, "irq_handler"), "CA65 symbol");

    /* Total should be substantial */
    TT_ASSERT(count >= 20, "multi-file should produce many symbols");

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * AUTOHOTKEY PARSER
 * ========================================================================== */

TT_TEST(test_ahk_functions)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_autohotkey(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);

    /* Top-level functions -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "CenterWindow"), "should find CenterWindow function");
    TT_ASSERT(has_sym(syms, count, "ShowNotify"), "should find ShowNotify function");

    const tt_symbol_t *s = find_sym(syms, count, "CenterWindow");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "winTitle");
    }

    s = find_sym(syms, count, "ShowNotify");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);
        /* Should capture params including defaults */
        TT_ASSERT_STR_CONTAINS(s->signature, "msg");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_classes)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    /* class WindowManager -> CLASS */
    TT_ASSERT(has_sym(syms, count, "WindowManager"), "should find WindowManager class");
    const tt_symbol_t *s = find_sym(syms, count, "WindowManager");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);

    /* class KeyLogger extends WindowManager -> CLASS */
    TT_ASSERT(has_sym(syms, count, "KeyLogger"), "should find KeyLogger class");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_methods)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    /* Methods inside class -> METHOD with qualified name */
    TT_ASSERT(has_sym(syms, count, "WindowManager.Minimize"), "should find Minimize method");
    TT_ASSERT(has_sym(syms, count, "WindowManager.Restore"), "should find Restore method");
    TT_ASSERT(has_sym(syms, count, "WindowManager.Toggle"), "should find Toggle method");
    TT_ASSERT(has_sym(syms, count, "WindowManager.GetOrCreate"), "should find static GetOrCreate");

    const tt_symbol_t *s = find_sym(syms, count, "WindowManager.Minimize");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_METHOD, s->kind);

    /* Subclass methods get their own class prefix */
    TT_ASSERT(has_sym(syms, count, "KeyLogger.LogKey"), "should find LogKey method in subclass");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_hotkeys)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    /* Top-level hotkeys -> CONSTANT */
    int hotkey_count = count_kind(syms, count, TT_KIND_CONSTANT);
    TT_ASSERT(hotkey_count >= 1, "should find at least one hotkey");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_hotif_directives)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    /* #HotIf directive -> PROPERTY */
    int prop_count = count_kind(syms, count, TT_KIND_PROPERTY);
    TT_ASSERT(prop_count >= 1, "should find at least one #HotIf directive");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_docstrings)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    /* ; comments before functions should be captured */
    const tt_symbol_t *s = find_sym(syms, count, "CenterWindow");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "center a window");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_language_field)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_autohotkey.ahk"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_autohotkey(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("autohotkey", syms[i].language);
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_ahk_empty_file)
{
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_autohotkey("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(0, count);
}

/* ==========================================================================
 * OPENAPI PARSER -- JSON Format
 * ========================================================================== */

TT_TEST(test_openapi_json_api_info)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.json"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_openapi(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT(count > 0, "openapi JSON should produce symbols");

    /* API title -> CLASS */
    TT_ASSERT(has_sym(syms, count, "Pet Store API"), "should find API title");
    const tt_symbol_t *s = find_sym(syms, count, "Pet Store API");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "1.2.0");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_json_operations)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.json"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_openapi(fixtures, files, 1, &syms, &count);

    /* Operations with operationId -> FUNCTION (named by operationId) */
    TT_ASSERT(has_sym(syms, count, "listPets"), "should find listPets operation");
    TT_ASSERT(has_sym(syms, count, "createPet"), "should find createPet operation");
    TT_ASSERT(has_sym(syms, count, "getPetById"), "should find getPetById operation");

    const tt_symbol_t *s = find_sym(syms, count, "listPets");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "GET /pets");
    }

    /* DELETE /pets/{petId} has no operationId -- named by method+path */
    TT_ASSERT(has_sym(syms, count, "DELETE /pets/{petId}"),
              "operation without operationId should use method+path");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_json_schemas)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.json"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_openapi(fixtures, files, 1, &syms, &count);

    /* Component schemas -> CLASS */
    TT_ASSERT(has_sym(syms, count, "Pet"), "should find Pet schema");
    TT_ASSERT(has_sym(syms, count, "Error"), "should find Error schema");

    const tt_symbol_t *s = find_sym(syms, count, "Pet");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "object");
        /* Docstring from description */
        if (s->docstring) {
            TT_ASSERT_STR_CONTAINS(s->docstring, "pet in the store");
        }
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_json_language_field)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.json"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_openapi(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("openapi", syms[i].language);
    }

    tt_symbol_array_free(syms, count);
}

/* ==========================================================================
 * OPENAPI PARSER -- YAML Format
 * ========================================================================== */

TT_TEST(test_openapi_yaml_api_info)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.yaml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_openapi(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT(count > 0, "openapi YAML should produce symbols");

    /* API title -> CLASS */
    TT_ASSERT(has_sym(syms, count, "User Service"), "should find API title from YAML");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_yaml_operations)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.yaml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_openapi(fixtures, files, 1, &syms, &count);

    /* Operations with operationId */
    TT_ASSERT(has_sym(syms, count, "listUsers"), "should find listUsers from YAML");
    TT_ASSERT(has_sym(syms, count, "createUser"), "should find createUser from YAML");
    TT_ASSERT(has_sym(syms, count, "getUserById"), "should find getUserById from YAML");
    TT_ASSERT(has_sym(syms, count, "updateUser"), "should find updateUser from YAML");
    TT_ASSERT(has_sym(syms, count, "deleteUser"), "should find deleteUser from YAML");

    /* All operations are FUNCTION kind */
    int func_count = count_kind(syms, count, TT_KIND_FUNCTION);
    TT_ASSERT_GE_INT(func_count, 5);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_yaml_schemas)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_openapi.yaml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_openapi(fixtures, files, 1, &syms, &count);

    TT_ASSERT(has_sym(syms, count, "User"), "should find User schema from YAML");
    TT_ASSERT(has_sym(syms, count, "Address"), "should find Address schema from YAML");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_openapi_empty_file)
{
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_openapi("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(0, count);
}

/* ==========================================================================
 * XML PARSER
 * ========================================================================== */

TT_TEST(test_xml_root_element)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_xml(fixtures, files, 1, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT(count > 0, "xml should produce symbols");

    /* Root element -> CLASS */
    TT_ASSERT(has_sym(syms, count, "application"), "should find root element 'application'");
    const tt_symbol_t *s = find_sym(syms, count, "application");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CLASS, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "<application>");
        /* Preceding comment should be docstring */
        if (s->docstring) {
            TT_ASSERT_STR_CONTAINS(s->docstring, "Application configuration");
        }
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_id_attribute)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* Elements with id attribute -> CONSTANT, qualified as tag::id */
    TT_ASSERT(has_sym(syms, count, "menu::main-nav"), "should find menu#main-nav");
    TT_ASSERT(has_sym(syms, count, "item::home"), "should find item#home");
    TT_ASSERT(has_sym(syms, count, "item::settings"), "should find item#settings");

    const tt_symbol_t *s = find_sym(syms, count, "menu::main-nav");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "id=\"main-nav\"");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_name_attribute_fallback)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* Elements with name attribute (no id) -> CONSTANT, qualified as tag::name */
    TT_ASSERT(has_sym(syms, count, "panel::user-prefs"), "should find panel with name attr");

    const tt_symbol_t *s = find_sym(syms, count, "panel::user-prefs");
    if (s) {
        TT_ASSERT_EQ_INT(TT_KIND_CONSTANT, s->kind);
        TT_ASSERT_STR_CONTAINS(s->signature, "name=\"user-prefs\"");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_key_attribute_fallback)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* Elements with key attribute (no id, no name) -> CONSTANT */
    TT_ASSERT(has_sym(syms, count, "input::theme"), "should find input with key=theme");
    TT_ASSERT(has_sym(syms, count, "input::language"), "should find input with key=language");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_script_src)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* <script src="..."> -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "js/app.js"), "should find script src js/app.js");
    TT_ASSERT(has_sym(syms, count, "js/utils.js"), "should find script src js/utils.js");

    const tt_symbol_t *s = find_sym(syms, count, "js/app.js");
    if (s) TT_ASSERT_EQ_INT(TT_KIND_FUNCTION, s->kind);

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_link_href)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* <link href="..."> -> FUNCTION */
    TT_ASSERT(has_sym(syms, count, "css/main.css"), "should find link href css/main.css");

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_docstrings)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    /* <!-- Main navigation menu --> before <menu> should be docstring */
    const tt_symbol_t *s = find_sym(syms, count, "menu::main-nav");
    TT_ASSERT_NOT_NULL(s);
    if (s && s->docstring) {
        TT_ASSERT_STR_CONTAINS(s->docstring, "navigation menu");
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_language_field)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"sample_xml.xml"};
    tt_symbol_t *syms = NULL;
    int count = 0;

    tt_parse_xml(fixtures, files, 1, &syms, &count);

    for (int i = 0; i < count; i++) {
        TT_ASSERT_EQ_STR("xml", syms[i].language);
    }

    tt_symbol_array_free(syms, count);
}

TT_TEST(test_xml_empty_file)
{
    tt_symbol_t *syms = NULL;
    int count = 0;

    int rc = tt_parse_xml("/tmp", NULL, 0, &syms, &count);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(0, count);
}

/* ==========================================================================
 * PUBLIC RUNNER
 * ========================================================================== */

void run_int_parser_extra_tests(void)
{
    TT_SUITE("Integration: ASM Parser (WLA-DX)");
    TT_RUN(test_asm_wladx_labels);
    TT_RUN(test_asm_wladx_local_labels_excluded);
    TT_RUN(test_asm_wladx_sections);
    TT_RUN(test_asm_wladx_ramsection);
    TT_RUN(test_asm_wladx_macros);
    TT_RUN(test_asm_wladx_constants);
    TT_RUN(test_asm_wladx_struct);
    TT_RUN(test_asm_wladx_enum_members);
    TT_RUN(test_asm_wladx_docstrings);
    TT_RUN(test_asm_wladx_language_field);

    TT_SUITE("Integration: ASM Parser (NASM)");
    TT_RUN(test_asm_nasm_labels);
    TT_RUN(test_asm_nasm_defines);
    TT_RUN(test_asm_nasm_macros);
    TT_RUN(test_asm_nasm_equ_constants);
    TT_RUN(test_asm_nasm_sections);
    TT_RUN(test_asm_nasm_docstring);

    TT_SUITE("Integration: ASM Parser (GAS)");
    TT_RUN(test_asm_gas_labels);
    TT_RUN(test_asm_gas_constants);
    TT_RUN(test_asm_gas_macros);
    TT_RUN(test_asm_gas_block_comments);

    TT_SUITE("Integration: ASM Parser (CA65)");
    TT_RUN(test_asm_ca65_procs);
    TT_RUN(test_asm_ca65_segments);
    TT_RUN(test_asm_ca65_constants);
    TT_RUN(test_asm_ca65_macros);
    TT_RUN(test_asm_ca65_docstrings);

    TT_SUITE("Integration: ASM Parser (edge cases)");
    TT_RUN(test_asm_empty_file);
    TT_RUN(test_asm_multi_file);

    TT_SUITE("Integration: AutoHotkey Parser");
    TT_RUN(test_ahk_functions);
    TT_RUN(test_ahk_classes);
    TT_RUN(test_ahk_methods);
    TT_RUN(test_ahk_hotkeys);
    TT_RUN(test_ahk_hotif_directives);
    TT_RUN(test_ahk_docstrings);
    TT_RUN(test_ahk_language_field);
    TT_RUN(test_ahk_empty_file);

    TT_SUITE("Integration: OpenAPI Parser (JSON)");
    TT_RUN(test_openapi_json_api_info);
    TT_RUN(test_openapi_json_operations);
    TT_RUN(test_openapi_json_schemas);
    TT_RUN(test_openapi_json_language_field);

    TT_SUITE("Integration: OpenAPI Parser (YAML)");
    TT_RUN(test_openapi_yaml_api_info);
    TT_RUN(test_openapi_yaml_operations);
    TT_RUN(test_openapi_yaml_schemas);
    TT_RUN(test_openapi_empty_file);

    TT_SUITE("Integration: XML Parser");
    TT_RUN(test_xml_root_element);
    TT_RUN(test_xml_id_attribute);
    TT_RUN(test_xml_name_attribute_fallback);
    TT_RUN(test_xml_key_attribute_fallback);
    TT_RUN(test_xml_script_src);
    TT_RUN(test_xml_link_href);
    TT_RUN(test_xml_docstrings);
    TT_RUN(test_xml_language_field);
    TT_RUN(test_xml_empty_file);
}
