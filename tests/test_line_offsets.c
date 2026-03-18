/*
 * test_line_offsets.c -- Unit tests for line_offsets module.
 */

#include "test_framework.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TT_TEST(test_lo_build_known_content)
{
    const char *content = "line1\nline2\nline3";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(3, lo.count);
    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));
    TT_ASSERT_EQ_INT(6, tt_line_offsets_offset_at(&lo, 2));
    TT_ASSERT_EQ_INT(12, tt_line_offsets_offset_at(&lo, 3));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_line_at)
{
    const char *content = "abc\ndef\nghi";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(1, tt_line_offsets_line_at(&lo, 0));
    TT_ASSERT_EQ_INT(1, tt_line_offsets_line_at(&lo, 2));
    TT_ASSERT_EQ_INT(2, tt_line_offsets_line_at(&lo, 4));
    TT_ASSERT_EQ_INT(3, tt_line_offsets_line_at(&lo, 8));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_offset_at)
{
    const char *content = "hello\nworld\n";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));
    TT_ASSERT_EQ_INT(6, tt_line_offsets_offset_at(&lo, 2));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_empty_content)
{
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, "", 0);

    TT_ASSERT_EQ_INT(1, lo.count);
    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_single_line_no_newline)
{
    const char *content = "no newline here";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(1, lo.count);
    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_single_line_with_trailing_newline)
{
    const char *content = "one line\n";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));
    TT_ASSERT_EQ_INT(9, tt_line_offsets_offset_at(&lo, 2));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_line_at_newline_boundary)
{
    const char *content = "ab\ncd\nef";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(1, tt_line_offsets_line_at(&lo, 2));  /* \n at end of line 1 */
    TT_ASSERT_EQ_INT(2, tt_line_offsets_line_at(&lo, 3));  /* 'c' at start of line 2 */
    TT_ASSERT_EQ_INT(2, tt_line_offsets_line_at(&lo, 5));  /* \n at end of line 2 */

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_line_at_last_byte)
{
    const char *content = "abc\ndef";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(2, tt_line_offsets_line_at(&lo, 6));  /* 'f' last byte */

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_crlf)
{
    const char *content = "line1\r\nline2\r\nline3";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));
    TT_ASSERT_EQ_INT(7, tt_line_offsets_offset_at(&lo, 2));
    TT_ASSERT_EQ_INT(14, tt_line_offsets_offset_at(&lo, 3));

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_consecutive_newlines)
{
    const char *content = "a\n\n\nb";
    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, content, strlen(content));

    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));  /* 'a' */
    TT_ASSERT_EQ_INT(2, tt_line_offsets_offset_at(&lo, 2));  /* empty */
    TT_ASSERT_EQ_INT(3, tt_line_offsets_offset_at(&lo, 3));  /* empty */
    TT_ASSERT_EQ_INT(4, tt_line_offsets_offset_at(&lo, 4));  /* 'b' */

    tt_line_offsets_free(&lo);
}

TT_TEST(test_lo_many_lines)
{
    char *buf = malloc(100000);
    TT_ASSERT_NOT_NULL(buf);
    int pos = 0;
    for (int i = 0; i < 1000; i++) {
        pos += snprintf(buf + pos, 100000 - pos, "line %d\n", i);
    }

    tt_line_offsets_t lo;
    tt_line_offsets_build(&lo, buf, (size_t)pos);

    TT_ASSERT_EQ_INT(1001, lo.count); /* 1000 lines + trailing empty */
    TT_ASSERT_EQ_INT(0, tt_line_offsets_offset_at(&lo, 1));
    TT_ASSERT_GT_INT(tt_line_offsets_offset_at(&lo, 1000), 0);
    TT_ASSERT_EQ_INT(1000, tt_line_offsets_line_at(&lo, tt_line_offsets_offset_at(&lo, 1000)));

    tt_line_offsets_free(&lo);
    free(buf);
}

void run_line_offsets_tests(void)
{
    TT_RUN(test_lo_build_known_content);
    TT_RUN(test_lo_line_at);
    TT_RUN(test_lo_offset_at);
    TT_RUN(test_lo_empty_content);
    TT_RUN(test_lo_single_line_no_newline);
    TT_RUN(test_lo_single_line_with_trailing_newline);
    TT_RUN(test_lo_line_at_newline_boundary);
    TT_RUN(test_lo_line_at_last_byte);
    TT_RUN(test_lo_crlf);
    TT_RUN(test_lo_consecutive_newlines);
    TT_RUN(test_lo_many_lines);
}
