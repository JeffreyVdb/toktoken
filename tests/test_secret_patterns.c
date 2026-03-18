/*
 * test_secret_patterns.c -- Unit tests for secret_patterns module.
 */

#include "test_framework.h"
#include "secret_patterns.h"

TT_TEST(test_secret_env_files)
{
    TT_ASSERT_TRUE(tt_is_secret(".env"));
    TT_ASSERT_TRUE(tt_is_secret(".env.local"));
    TT_ASSERT_TRUE(tt_is_secret(".env.production"));
    TT_ASSERT_TRUE(tt_is_secret(".env.staging"));
    TT_ASSERT_TRUE(tt_is_secret("production.env"));
    TT_ASSERT_TRUE(tt_is_secret("staging.env"));
}

TT_TEST(test_secret_certificates)
{
    const char *exts[] = {"pem", "key", "p12", "pfx", "jks", "keystore", "crt", "cer"};
    for (int i = 0; i < 8; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "server.%s", exts[i]);
        TT_ASSERT(tt_is_secret(buf), buf);
    }
}

TT_TEST(test_secret_ssh_keys)
{
    TT_ASSERT_TRUE(tt_is_secret("id_rsa"));
    TT_ASSERT_TRUE(tt_is_secret("id_rsa.pub"));
    TT_ASSERT_TRUE(tt_is_secret("id_dsa"));
    TT_ASSERT_TRUE(tt_is_secret("id_dsa.pub"));
    TT_ASSERT_TRUE(tt_is_secret("id_ecdsa"));
    TT_ASSERT_TRUE(tt_is_secret("id_ecdsa.pub"));
    TT_ASSERT_TRUE(tt_is_secret("id_ed25519"));
    TT_ASSERT_TRUE(tt_is_secret("id_ed25519.pub"));
}

TT_TEST(test_secret_credentials)
{
    TT_ASSERT_TRUE(tt_is_secret("credentials.json"));
    TT_ASSERT_TRUE(tt_is_secret("app.secret"));
    TT_ASSERT_TRUE(tt_is_secret("auth.token"));
    TT_ASSERT_TRUE(tt_is_secret("master.key"));
}

TT_TEST(test_secret_dotfiles)
{
    TT_ASSERT_TRUE(tt_is_secret(".htpasswd"));
    TT_ASSERT_TRUE(tt_is_secret(".htaccess"));
    TT_ASSERT_TRUE(tt_is_secret(".netrc"));
    TT_ASSERT_TRUE(tt_is_secret(".npmrc"));
    TT_ASSERT_TRUE(tt_is_secret(".pypirc"));
}

TT_TEST(test_secret_config_files)
{
    TT_ASSERT_TRUE(tt_is_secret("wp-config.php"));
    TT_ASSERT_TRUE(tt_is_secret("database.yml"));
}

TT_TEST(test_non_secret_normal_files)
{
    TT_ASSERT_FALSE(tt_is_secret("App.php"));
    TT_ASSERT_FALSE(tt_is_secret("index.js"));
    TT_ASSERT_FALSE(tt_is_secret("README.md"));
    TT_ASSERT_FALSE(tt_is_secret("config.json"));
    TT_ASSERT_FALSE(tt_is_secret("main.go"));
}

TT_TEST(test_secret_broad_pattern_skipped_for_docs)
{
    TT_ASSERT_FALSE(tt_is_secret("secret-handling.md"));
    const char *doc_exts[] = {"md", "markdown", "mdx", "rst", "txt", "adoc", "html", "htm"};
    for (int i = 0; i < 8; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "my-secret-docs.%s", doc_exts[i]);
        TT_ASSERT(!tt_is_secret(buf), buf);
    }
}

TT_TEST(test_secret_broad_pattern_skipped_for_code)
{
    /* Broad pattern *secret* is skipped for source code extensions */
    TT_ASSERT_FALSE(tt_is_secret("secret-config.php"));
    TT_ASSERT_FALSE(tt_is_secret("my-secret-handler.py"));
    TT_ASSERT_FALSE(tt_is_secret("Secret.php"));
    TT_ASSERT_FALSE(tt_is_secret("SecretManager.java"));
    TT_ASSERT_FALSE(tt_is_secret("secret_handler.go"));
    TT_ASSERT_FALSE(tt_is_secret("secret.js"));
    TT_ASSERT_FALSE(tt_is_secret("secret.rs"));
    /* But broad pattern still applies to non-code/non-doc extensions */
    TT_ASSERT_TRUE(tt_is_secret("secret-config.yml"));
    TT_ASSERT_TRUE(tt_is_secret("secret-config.json"));
    TT_ASSERT_TRUE(tt_is_secret("secret-config.ini"));
}

TT_TEST(test_secret_nested_path_uses_basename)
{
    TT_ASSERT_TRUE(tt_is_secret(".env"));
    TT_ASSERT_FALSE(tt_is_secret("App.php"));
}

TT_TEST(test_non_secret_with_similar_names)
{
    TT_ASSERT_FALSE(tt_is_secret("env.php"));
    TT_ASSERT_FALSE(tt_is_secret("EnvLoader.php"));
    TT_ASSERT_FALSE(tt_is_secret("package.json"));
}

void run_secret_patterns_tests(void)
{
    TT_RUN(test_secret_env_files);
    TT_RUN(test_secret_certificates);
    TT_RUN(test_secret_ssh_keys);
    TT_RUN(test_secret_credentials);
    TT_RUN(test_secret_dotfiles);
    TT_RUN(test_secret_config_files);
    TT_RUN(test_non_secret_normal_files);
    TT_RUN(test_secret_broad_pattern_skipped_for_docs);
    TT_RUN(test_secret_broad_pattern_skipped_for_code);
    TT_RUN(test_secret_nested_path_uses_basename);
    TT_RUN(test_non_secret_with_similar_names);
}
