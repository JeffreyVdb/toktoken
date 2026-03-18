/*
 * test_import_extractor.c -- Unit tests for import extraction.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "import_extractor.h"

#include <string.h>

/* Helper: find an import by specifier */
static const tt_import_t *find_import(const tt_import_t *imps, int count,
                                       const char *specifier)
{
    for (int i = 0; i < count; i++) {
        if (imps[i].to_specifier && strcmp(imps[i].to_specifier, specifier) == 0)
            return &imps[i];
    }
    return NULL;
}

TT_TEST(test_extract_js_imports)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/imports", fixtures);

    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports(root, "app.js", "javascript", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count >= 4, "JS has at least 4 imports");

    const tt_import_t *imp = find_import(imps, count, "react");
    TT_ASSERT(imp != NULL, "found react import");
    if (imp) TT_ASSERT_EQ_STR(imp->import_type, "import");

    imp = find_import(imps, count, "./styles.css");
    TT_ASSERT(imp != NULL, "found styles.css side-effect import");

    imp = find_import(imps, count, "fs");
    TT_ASSERT(imp != NULL, "found fs require");
    if (imp) TT_ASSERT_EQ_STR(imp->import_type, "require");

    imp = find_import(imps, count, "path");
    TT_ASSERT(imp != NULL, "found path require");

    tt_import_array_free(imps, count);
}

TT_TEST(test_extract_python_imports)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/imports", fixtures);

    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports(root, "main.py", "python", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count >= 4, "Python has at least 4 imports");

    const tt_import_t *imp = find_import(imps, count, "os");
    TT_ASSERT(imp != NULL, "found os import");
    if (imp) TT_ASSERT_EQ_STR(imp->import_type, "import");

    imp = find_import(imps, count, "pathlib");
    TT_ASSERT(imp != NULL, "found pathlib from-import");
    if (imp) {
        TT_ASSERT_EQ_STR(imp->import_type, "from");
        TT_ASSERT(imp->symbol_name != NULL, "has symbol_name");
        if (imp->symbol_name) TT_ASSERT_EQ_STR(imp->symbol_name, "Path");
    }

    tt_import_array_free(imps, count);
}

TT_TEST(test_extract_go_imports)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/imports", fixtures);

    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports(root, "main.go", "go", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count >= 4, "Go has at least 4 imports");

    const tt_import_t *imp = find_import(imps, count, "fmt");
    TT_ASSERT(imp != NULL, "found fmt import");

    imp = find_import(imps, count, "net/http");
    TT_ASSERT(imp != NULL, "found net/http import");

    tt_import_array_free(imps, count);
}

TT_TEST(test_extract_c_includes)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/imports", fixtures);

    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports(root, "main.c", "c", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count >= 4, "C has at least 4 includes");

    const tt_import_t *imp = find_import(imps, count, "stdio.h");
    TT_ASSERT(imp != NULL, "found <stdio.h> include");
    if (imp) TT_ASSERT_EQ_STR(imp->import_type, "include");

    imp = find_import(imps, count, "myheader.h");
    TT_ASSERT(imp != NULL, "found \"myheader.h\" include");

    tt_import_array_free(imps, count);
}

TT_TEST(test_extract_php_imports)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    char root[512];
    snprintf(root, sizeof(root), "%s/imports", fixtures);

    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports(root, "App.php", "php", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT(count >= 3, "PHP has at least 3 imports");

    const tt_import_t *imp = find_import(imps, count, "App\\Controller\\MainController");
    TT_ASSERT(imp != NULL, "found use MainController");
    if (imp) {
        TT_ASSERT_EQ_STR(imp->import_type, "use");
        TT_ASSERT(imp->symbol_name != NULL, "has symbol_name");
        if (imp->symbol_name) TT_ASSERT_EQ_STR(imp->symbol_name, "MainController");
    }

    tt_import_array_free(imps, count);
}

TT_TEST(test_extract_unsupported_language)
{
    tt_import_t *imps = NULL;
    int count = 0;
    int rc = tt_extract_imports("/tmp", "test.xyz", "unknown_lang", &imps, &count);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(count, 0);
}

TT_TEST(test_extract_null_guards)
{
    tt_import_t *imps = NULL;
    int count = 0;

    TT_ASSERT_EQ_INT(tt_extract_imports(NULL, "f", "c", &imps, &count), 0);
    TT_ASSERT_EQ_INT(count, 0);

    TT_ASSERT_EQ_INT(tt_extract_imports("/tmp", NULL, "c", &imps, &count), 0);
    TT_ASSERT_EQ_INT(count, 0);

    TT_ASSERT_EQ_INT(tt_extract_imports("/tmp", "f", NULL, &imps, &count), 0);
    TT_ASSERT_EQ_INT(count, 0);

    TT_ASSERT_EQ_INT(tt_extract_imports("/tmp", "f", "c", NULL, &count), -1);
}

/* ---- Public runner ---- */

void run_import_extractor_tests(void)
{
    TT_SUITE("Import Extractor: JavaScript");
    TT_RUN(test_extract_js_imports);

    TT_SUITE("Import Extractor: Python");
    TT_RUN(test_extract_python_imports);

    TT_SUITE("Import Extractor: Go");
    TT_RUN(test_extract_go_imports);

    TT_SUITE("Import Extractor: C");
    TT_RUN(test_extract_c_includes);

    TT_SUITE("Import Extractor: PHP");
    TT_RUN(test_extract_php_imports);

    TT_SUITE("Import Extractor: Edge Cases");
    TT_RUN(test_extract_unsupported_language);
    TT_RUN(test_extract_null_guards);
}
