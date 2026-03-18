/*
 * secret_patterns.c -- Detection of secret/credential files.
 */

#include "secret_patterns.h"
#include "platform.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* secret patterns */
static const char *PATTERNS[] = {
    /* Environment files */
    ".env",
    ".env.*",
    "*.env",
    /* Certificates and keys */
    "*.pem",
    "*.key",
    "*.p12",
    "*.pfx",
    "*.jks",
    "*.keystore",
    "*.crt",
    "*.cer",
    /* SSH keys */
    "id_rsa",
    "id_rsa.*",
    "id_dsa",
    "id_dsa.*",
    "id_ecdsa",
    "id_ecdsa.*",
    "id_ed25519",
    "id_ed25519.*",
    /* Credentials and secrets */
    "credentials.json",
    "service-account*.json",
    "*.credentials",
    "secrets.*",
    "*.secret",
    "*.secrets",
    "*secret*",
    "*.token",
    /* Auth and config files */
    ".htpasswd",
    ".htaccess",
    ".netrc",
    ".npmrc",
    ".pypirc",
    "wp-config.php",
    "database.yml",
    "master.key",
    NULL};

/* Broad patterns that are excluded for documentation files */
static const char *BROAD_PATTERNS[] = {
    "*secret*",
    NULL};

/* Documentation extensions (11 entries) */
static const char *DOC_EXTENSIONS[] = {
    "md", "markdown", "mdx", "rst", "txt",
    "adoc", "asciidoc", "asc", "html", "htm", "ipynb",
    NULL};

/* Source code extensions where broad patterns produce false positives */
static const char *CODE_EXTENSIONS[] = {
    "php", "py", "rb", "java", "kt", "scala", "cs", "go", "rs", "swift",
    "c", "cpp", "h", "hpp", "js", "ts", "jsx", "tsx", "vue", "svelte",
    "lua", "pl", "pm", "r", "ex", "exs", "erl", "hs", "clj", "dart",
    "m", "mm", "f90", "f95", "jl",
    "asm", "s",
    NULL};

static bool is_code_extension(const char *ext_lower)
{
    for (const char **ce = CODE_EXTENSIONS; *ce; ce++)
    {
        if (strcmp(ext_lower, *ce) == 0)
            return true;
    }
    return false;
}

static bool is_broad(const char *pattern)
{
    for (const char **bp = BROAD_PATTERNS; *bp; bp++)
    {
        if (strcmp(pattern, *bp) == 0)
            return true;
    }
    return false;
}

static bool is_doc_extension(const char *ext_lower)
{
    for (const char **de = DOC_EXTENSIONS; *de; de++)
    {
        if (strcmp(ext_lower, *de) == 0)
            return true;
    }
    return false;
}

bool tt_is_secret(const char *filename)
{
    if (!filename || !*filename)
        return false;

    /* Extract extension (lowercase) */
    const char *dot = strrchr(filename, '.');
    char ext_lower[32] = "";
    if (dot && dot != filename)
    {
        size_t elen = strlen(dot + 1);
        if (elen < sizeof(ext_lower))
        {
            for (size_t i = 0; i <= elen; i++)
                ext_lower[i] = (char)tolower((unsigned char)dot[1 + i]);
        }
    }

    bool is_doc = is_doc_extension(ext_lower);
    bool is_code = is_code_extension(ext_lower);

    /* Lowercase the basename for case-insensitive matching */
    char *base_lower = tt_str_tolower(filename);
    if (!base_lower)
        return false;

    for (const char **p = PATTERNS; *p; p++)
    {
        if ((is_doc || is_code) && is_broad(*p))
            continue;
        if (tt_fnmatch(*p, base_lower, true))
        {
            free(base_lower);
            return true;
        }
    }

    free(base_lower);
    return false;
}
