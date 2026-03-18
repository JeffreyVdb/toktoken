## What does this PR do?

<!-- Describe the change in 1-3 sentences. Link the related issue if applicable: Fixes #123 -->

## Type of change

- [ ] Bug fix
- [ ] New feature
- [ ] Language support (new or improved parser)
- [ ] Performance improvement
- [ ] Documentation
- [ ] Refactoring (no functional change)

## Checklist

- [ ] `cmake --build build` compiles with zero warnings
- [ ] All tests pass (`cd build && ctest`)
- [ ] New code has test coverage

### If this PR modifies the schema:

- [ ] Bumped `TT_SCHEMA_VERSION` in `schema.h`
- [ ] Added migration in `tt_schema_migrate()`
- [ ] Updated `docs/ARCHITECTURE.md` schema section

### If this PR adds/modifies MCP tools:

- [ ] Updated tool count and descriptions in `README.md`
- [ ] Updated `docs/LLM.md` (commands, MCP tools table, workflow examples)
- [ ] Updated `docs/setup/claude-code.md` permission list

### If this PR adds a custom parser:

- [ ] Parser handles malformed input without crashing
- [ ] Extensions registered in `language_detector.c`
- [ ] Added to `docs/LANGUAGES.md` custom parsers table

## How to test

<!-- Steps to verify the change works correctly. -->
