/*
 * test_summarizer.c -- Unit tests for summarizer module (Tier 1 + Tier 3).
 */

#include "test_framework.h"
#include "summarizer.h"
#include "symbol_kind.h"

#include <stdlib.h>
#include <string.h>

/* ---- Docstring summarizer (Tier 1) ---- */

TT_TEST(test_sum_doc_first_sentence)
{
    char *s = tt_summarize_docstring("Create a new instance. This handles initialization.");
    TT_ASSERT_EQ_STR("Create a new instance.", s);
    free(s);
}

TT_TEST(test_sum_doc_truncation)
{
    char buf[300];
    memset(buf, 'a', 200);
    buf[200] = '\0';
    char *s = tt_summarize_docstring(buf);
    TT_ASSERT_LE_INT((int)strlen(s), 120);
    TT_ASSERT_STR_ENDS_WITH(s, "...");
    free(s);
}

TT_TEST(test_sum_doc_stops_at_tags)
{
    char *s = tt_summarize_docstring("Get the user name.\n@param int $id\n@return string");
    TT_ASSERT_EQ_STR("Get the user name.", s);
    free(s);
}

TT_TEST(test_sum_doc_empty_string)
{
    char *s = tt_summarize_docstring("");
    TT_ASSERT_EQ_STR("", s);
    free(s);
}

TT_TEST(test_sum_doc_whitespace_only)
{
    char *s = tt_summarize_docstring("   ");
    TT_ASSERT_EQ_STR("", s);
    free(s);
}

TT_TEST(test_sum_doc_multiline)
{
    char *s = tt_summarize_docstring("First line summary.\nSecond line detail.");
    TT_ASSERT_EQ_STR("First line summary.", s);
    free(s);
}

TT_TEST(test_sum_doc_only_tags)
{
    char *s = tt_summarize_docstring("@param int $id\n@return string");
    TT_ASSERT_EQ_STR("", s);
    free(s);
}

TT_TEST(test_sum_doc_exactly_120_not_truncated)
{
    char buf[121];
    memset(buf, 'x', 120);
    buf[120] = '\0';
    char *s = tt_summarize_docstring(buf);
    TT_ASSERT_EQ_INT(120, (int)strlen(s));
    TT_ASSERT_STR_NOT_CONTAINS(s, "...");
    free(s);
}

TT_TEST(test_sum_doc_exactly_121_truncated)
{
    char buf[122];
    memset(buf, 'x', 121);
    buf[121] = '\0';
    char *s = tt_summarize_docstring(buf);
    TT_ASSERT_EQ_INT(120, (int)strlen(s));
    TT_ASSERT_STR_ENDS_WITH(s, "...");
    free(s);
}

TT_TEST(test_sum_doc_no_space_after_dot)
{
    char *s = tt_summarize_docstring("Version 2.0 is released.");
    TT_ASSERT_EQ_STR("Version 2.0 is released.", s);
    free(s);
}

TT_TEST(test_sum_doc_stops_at_blank_line)
{
    char *s = tt_summarize_docstring("Summary line.\n\nDetailed description.");
    TT_ASSERT_EQ_STR("Summary line.", s);
    free(s);
}

TT_TEST(test_sum_doc_newlines_only)
{
    char *s = tt_summarize_docstring("\n\n\n");
    TT_ASSERT_EQ_STR("", s);
    free(s);
}

TT_TEST(test_sum_doc_single_word)
{
    char *s = tt_summarize_docstring("Constructor");
    TT_ASSERT_EQ_STR("Constructor", s);
    free(s);
}

TT_TEST(test_sum_doc_tag_on_first_line)
{
    char *s = tt_summarize_docstring("@deprecated Use newMethod instead.");
    TT_ASSERT_EQ_STR("", s);
    free(s);
}

/* ---- Signature summarizer (Tier 3) ---- */

TT_TEST(test_sum_sig_class)
{
    char *s = tt_summarize_signature(TT_KIND_CLASS, "AuthService");
    TT_ASSERT_EQ_STR("Class AuthService", s);
    free(s);
}

TT_TEST(test_sum_sig_method)
{
    char *s = tt_summarize_signature(TT_KIND_METHOD, "login");
    TT_ASSERT_EQ_STR("Method login", s);
    free(s);
}

TT_TEST(test_sum_sig_function)
{
    char *s = tt_summarize_signature(TT_KIND_FUNCTION, "helper");
    TT_ASSERT_EQ_STR("Function helper", s);
    free(s);
}

TT_TEST(test_sum_sig_interface)
{
    char *s = tt_summarize_signature(TT_KIND_INTERFACE, "Loggable");
    TT_ASSERT_EQ_STR("Interface Loggable", s);
    free(s);
}

TT_TEST(test_sum_sig_all_kinds)
{
    const struct { tt_symbol_kind_e kind; const char *label; } cases[] = {
        { TT_KIND_CLASS,     "Class" },
        { TT_KIND_INTERFACE, "Interface" },
        { TT_KIND_TRAIT,     "Trait" },
        { TT_KIND_ENUM,      "Enum" },
        { TT_KIND_FUNCTION,  "Function" },
        { TT_KIND_METHOD,    "Method" },
        { TT_KIND_CONSTANT,  "Constant" },
        { TT_KIND_PROPERTY,  "Property" },
        { TT_KIND_VARIABLE,  "Variable" },
        { TT_KIND_NAMESPACE, "Namespace" },
        { TT_KIND_TYPE,      "Type definition" },
        { TT_KIND_DIRECTIVE, "Directive" },
    };
    for (int i = 0; i < 12; i++) {
        char *s = tt_summarize_signature(cases[i].kind, "Foo");
        char expected[128];
        snprintf(expected, sizeof(expected), "%s Foo", cases[i].label);
        TT_ASSERT_EQ_STR(expected, s);
        free(s);
    }
}

/* ---- isTier3Fallback ---- */

TT_TEST(test_sum_tier3_class_label)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("Class User", TT_KIND_CLASS, "User"));
}

TT_TEST(test_sum_tier3_method_label)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("Method handle", TT_KIND_METHOD, "handle"));
}

TT_TEST(test_sum_tier3_empty_summary)
{
    TT_ASSERT_TRUE(tt_is_tier3_fallback("", TT_KIND_CLASS, "Foo"));
}

TT_TEST(test_sum_tier3_real_summary)
{
    TT_ASSERT_FALSE(tt_is_tier3_fallback("Handles user authentication.", TT_KIND_CLASS, "AuthService"));
}

TT_TEST(test_sum_tier3_similar_but_different)
{
    TT_ASSERT_FALSE(tt_is_tier3_fallback("Class User with extras", TT_KIND_CLASS, "User"));
}

void run_summarizer_tests(void)
{
    TT_RUN(test_sum_doc_first_sentence);
    TT_RUN(test_sum_doc_truncation);
    TT_RUN(test_sum_doc_stops_at_tags);
    TT_RUN(test_sum_doc_empty_string);
    TT_RUN(test_sum_doc_whitespace_only);
    TT_RUN(test_sum_doc_multiline);
    TT_RUN(test_sum_doc_only_tags);
    TT_RUN(test_sum_doc_exactly_120_not_truncated);
    TT_RUN(test_sum_doc_exactly_121_truncated);
    TT_RUN(test_sum_doc_no_space_after_dot);
    TT_RUN(test_sum_doc_stops_at_blank_line);
    TT_RUN(test_sum_doc_newlines_only);
    TT_RUN(test_sum_doc_single_word);
    TT_RUN(test_sum_doc_tag_on_first_line);
    TT_RUN(test_sum_sig_class);
    TT_RUN(test_sum_sig_method);
    TT_RUN(test_sum_sig_function);
    TT_RUN(test_sum_sig_interface);
    TT_RUN(test_sum_sig_all_kinds);
    TT_RUN(test_sum_tier3_class_label);
    TT_RUN(test_sum_tier3_method_label);
    TT_RUN(test_sum_tier3_empty_summary);
    TT_RUN(test_sum_tier3_real_summary);
    TT_RUN(test_sum_tier3_similar_but_different);
}
