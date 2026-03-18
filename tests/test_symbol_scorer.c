/*
 * test_symbol_scorer.c -- Unit tests for symbol_scorer module.
 */

#include "test_framework.h"
#include "symbol_scorer.h"

TT_TEST(test_scorer_exact_name)
{
    const char *words[] = {"login"};
    int score = tt_score_symbol("login", "login", "", "", "[]", "",
                                 "login", words, 1);
    TT_ASSERT_GE_INT(score, 20);
}

TT_TEST(test_scorer_substring)
{
    const char *words[] = {"login"};
    int score = tt_score_symbol("handleLogin", "handleLogin", "", "", "[]", "",
                                 "login", words, 1);
    TT_ASSERT_GE_INT(score, 10);
}

TT_TEST(test_scorer_word_overlap)
{
    const char *words[] = {"auth"};
    int score = tt_score_symbol("user_auth", "user_auth", "", "", "[]", "",
                                 "auth", words, 1);
    TT_ASSERT_GT_INT(score, 0);
}

TT_TEST(test_scorer_no_match)
{
    const char *words[] = {"auth"};
    int score = tt_score_symbol("calculator", "calculator", "", "", "[]", "",
                                 "auth", words, 1);
    TT_ASSERT_EQ_INT(0, score);
}

TT_TEST(test_scorer_signature_match)
{
    const char *words[] = {"auth"};
    int score = tt_score_symbol("run", "run",
                                 "public function run(AuthService $auth)",
                                 "", "[]", "",
                                 "auth", words, 1);
    TT_ASSERT_GT_INT(score, 0);
}

TT_TEST(test_scorer_keyword_match)
{
    const char *words[] = {"controller"};
    int score = tt_score_symbol("UserController", "UserController",
                                 "", "", "[\"user\",\"controller\"]", "",
                                 "controller", words, 1);
    TT_ASSERT_GT_INT(score, 0);
}

TT_TEST(test_scorer_exact_higher_than_substring)
{
    const char *words_auth[] = {"auth"};
    int exact = tt_score_symbol("auth", "auth", "", "", "[]", "",
                                 "auth", words_auth, 1);
    int substring = tt_score_symbol("handleAuth", "handleAuth", "", "", "[]", "",
                                     "auth", words_auth, 1);
    TT_ASSERT_GT_INT(exact, substring);
}

TT_TEST(test_scorer_case_insensitive)
{
    const char *words[] = {"authservice"};
    int score = tt_score_symbol("AuthService", "AuthService", "", "", "[]", "",
                                 "authservice", words, 1);
    TT_ASSERT_GE_INT(score, 20);
}

TT_TEST(test_scorer_qualified_name_exact)
{
    const char *words[] = {"auth.login"};
    int score = tt_score_symbol("login", "Auth.login", "", "", "[]", "",
                                 "auth.login", words, 1);
    TT_ASSERT_GE_INT(score, 20);
}

TT_TEST(test_scorer_summary_match)
{
    const char *words[] = {"authentication"};
    int score = tt_score_symbol("foo", "foo",
                                 "", "Handles user authentication", "[]", "",
                                 "authentication", words, 1);
    TT_ASSERT_GT_INT(score, 0);
}

TT_TEST(test_scorer_docstring_match)
{
    const char *words[] = {"payment"};
    int score = tt_score_symbol("x", "x",
                                 "", "", "[]",
                                 "Process the payment gateway response",
                                 "payment", words, 1);
    TT_ASSERT_GT_INT(score, 0);
}

TT_TEST(test_scorer_empty_row_zero)
{
    const char *words[] = {"auth"};
    int score = tt_score_symbol("", "", "", "", "[]", "",
                                 "auth", words, 1);
    TT_ASSERT_EQ_INT(0, score);
}

TT_TEST(test_scorer_invalid_keywords_json)
{
    const char *words[] = {"test"};
    int score = tt_score_symbol("test", "test", "", "", "not-json", "",
                                 "test", words, 1);
    TT_ASSERT_GT_INT(score, 0); /* Should still match on name */
}

TT_TEST(test_scorer_all_fields_contribute)
{
    const char *words[] = {"auth"};
    int score_all = tt_score_symbol(
        "auth", "App.auth",
        "function auth()", "Authenticate the auth user",
        "[\"auth\"]", "Auth method for auth",
        "auth", words, 1);
    int score_name = tt_score_symbol(
        "auth", "auth", "", "", "[]", "",
        "auth", words, 1);
    TT_ASSERT_GT_INT(score_all, score_name);
}

void run_symbol_scorer_tests(void)
{
    TT_RUN(test_scorer_exact_name);
    TT_RUN(test_scorer_substring);
    TT_RUN(test_scorer_word_overlap);
    TT_RUN(test_scorer_no_match);
    TT_RUN(test_scorer_signature_match);
    TT_RUN(test_scorer_keyword_match);
    TT_RUN(test_scorer_exact_higher_than_substring);
    TT_RUN(test_scorer_case_insensitive);
    TT_RUN(test_scorer_qualified_name_exact);
    TT_RUN(test_scorer_summary_match);
    TT_RUN(test_scorer_docstring_match);
    TT_RUN(test_scorer_empty_row_zero);
    TT_RUN(test_scorer_invalid_keywords_json);
    TT_RUN(test_scorer_all_fields_contribute);
}
