# TokToken -- AI Agent Setup & Integration Guide

You are an AI coding agent reading this document. Follow the steps below **in order** to install TokToken, configure your agent platform, and start using it. Every step is designed to be executed literally. Do not improvise or skip steps.

TokToken is a single static binary that indexes source code via universal-ctags, stores symbols in SQLite/FTS5, and exposes fast search/inspect commands. It saves 80-95% of context tokens compared to reading raw files.

---

## Step 1: Check if TokToken is already installed

```bash
test -x ~/.local/bin/toktoken && ~/.local/bin/toktoken --version
```

If this prints a version string, skip to **Step 3**. Otherwise continue.

---

## Step 2: Install TokToken

### 2.1 Detect platform and download

Use the mapping table below. Do NOT rely on `uname` output directly -- the binary names do not match `uname` values on all platforms.

| `uname -s` | `uname -m` | Binary name |
| ----------- | ---------- | ----------- |
| `Linux` | `x86_64` | `toktoken-linux-x86_64` |
| `Linux` | `aarch64` | `toktoken-linux-aarch64` |
| `Linux` | `armv7l` | `toktoken-linux-armv7` |
| `Darwin` | `x86_64` | `toktoken-macos-x86_64` |
| `Darwin` | `arm64` | `toktoken-macos-aarch64` |

Windows binary: `toktoken-win-x86_64.exe` (download manually or via PowerShell -- see below).

Download using the `latest` release URL (no version number needed):

```bash
# Example for Linux x86_64:
mkdir -p ~/.local/bin
curl -fsSL https://github.com/mauriziofonte/toktoken/releases/latest/download/toktoken-linux-x86_64 \
  -o ~/.local/bin/toktoken && chmod +x ~/.local/bin/toktoken
```

Full platform detection script (copy-paste safe):

```bash
case "$(uname -s)" in
    Linux)  TT_OS="linux" ;;
    Darwin) TT_OS="macos" ;;
    *)      echo "Unsupported OS"; exit 1 ;;
esac

case "$(uname -m)" in
    x86_64)         TT_ARCH="x86_64" ;;
    aarch64|arm64)  TT_ARCH="aarch64" ;;
    armv7l|armv7)   TT_ARCH="armv7" ;;
    *)              echo "Unsupported architecture"; exit 1 ;;
esac

TT_BINARY="toktoken-${TT_OS}-${TT_ARCH}"
TT_URL="https://github.com/mauriziofonte/toktoken/releases/latest/download/${TT_BINARY}"

# Install to ~/.local/bin (user-writable, no sudo needed)
mkdir -p ~/.local/bin
curl -fsSL "$TT_URL" -o ~/.local/bin/toktoken && chmod +x ~/.local/bin/toktoken
```

On macOS, remove the quarantine attribute after download:

```bash
xattr -d com.apple.quarantine ~/.local/bin/toktoken 2>/dev/null || true
```

On Windows (PowerShell):

```powershell
New-Item -ItemType Directory -Force -Path "$env:USERPROFILE\.local\bin" | Out-Null
Invoke-WebRequest -Uri "https://github.com/mauriziofonte/toktoken/releases/latest/download/toktoken-win-x86_64.exe" `
  -OutFile "$env:USERPROFILE\.local\bin\toktoken.exe"
```

Then add `%USERPROFILE%\.local\bin` to your PATH via System Settings > Environment Variables, or run:

```powershell
$binPath = "$env:USERPROFILE\.local\bin"
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($currentPath -notlike "*$binPath*") {
    [Environment]::SetEnvironmentVariable("PATH", "$binPath;$currentPath", "User")
}
```

### 2.2 Ensure `~/.local/bin` is in PATH (Linux/macOS)

```bash
case ":$PATH:" in
    *":$HOME/.local/bin:"*) ;; # already in PATH
    *) echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.profile
       export PATH="$HOME/.local/bin:$PATH" ;;
esac
```

### 2.3 Verify installation

```bash
toktoken --version
```

This must print `toktoken X.Y.Z`. If it fails, the binary is not in PATH or the wrong platform binary was downloaded.

### 2.4 Install universal-ctags

TokToken requires `universal-ctags` (NOT exuberant-ctags) for symbol extraction.

You're very likely **not** allowed to run `sudo` commands on behalf of the User, so, it that's the case, **do not run the install commands below**. Instead, inform the User that they need to install universal-ctags themselves, and provide them with the appropriate command for their platform from the table below.

```bash
# Check if already installed
ctags --version 2>/dev/null | head -1
# Must show "Universal Ctags". If it shows "Exuberant Ctags" or nothing, install:

# Ubuntu/Debian
sudo apt-get install -y universal-ctags

# macOS
brew install universal-ctags

# Arch
sudo pacman -S ctags

# Fedora/RHEL
sudo dnf install universal-ctags
```

---

## Step 3: Configure agent integration

TokToken supports two integration modes:

- **MCP server** (preferred): `toktoken serve` -- the agent calls tools directly via Model Context Protocol
- **CLI fallback**: the agent runs shell commands like `toktoken search:symbols "query"`

### 3.1 MCP server configuration

Find your agent below and follow the linked setup guide:

| Agent | Config format | Setup guide |
| ----- | ------------- | ----------- |
| **Claude Code** | CLI registration | [docs/setup/claude-code.md](setup/claude-code.md) |
| **Claude Desktop** | `"mcpServers"` JSON | [docs/setup/claude-desktop.md](setup/claude-desktop.md) |
| **VS Code / GitHub Copilot** | `"servers"` JSON | [docs/setup/copilot-vscode.md](setup/copilot-vscode.md) |
| **Cursor** | `"mcpServers"` JSON | [docs/setup/cursor.md](setup/cursor.md) |
| **Windsurf** | `"mcpServers"` JSON | [docs/setup/windsurf.md](setup/windsurf.md) |
| **Gemini CLI** | `"mcpServers"` JSON | [docs/setup/gemini-cli.md](setup/gemini-cli.md) |
| **Gemini Code Assist** | `"mcpServers"` JSON | [docs/setup/gemini-code-assist.md](setup/gemini-code-assist.md) |
| **OpenAI Codex CLI** | Rules file (no MCP) | [docs/setup/codex-cli.md](setup/codex-cli.md) |

> **Warning:** VS Code / GitHub Copilot uses `"servers"` as the top-level key, **not** `"mcpServers"`. Using the wrong format will silently fail. See [copilot-vscode.md](setup/copilot-vscode.md) for the correct configuration.

For most MCP clients (all except Claude Code, VS Code, and Codex CLI), the JSON configuration follows this pattern:

```json
{
    "mcpServers": {
        "toktoken": {
            "command": "toktoken",
            "args": ["serve"]
        }
    }
}
```

**If the config file already exists, READ it first, MERGE the toktoken entry into the existing object, and WRITE back. Do NOT overwrite the file.**

### 3.2 CLI rules setup (for agents without MCP, or as complement)

Copy the rules template below into your agent's instructions file. Each vendor setup guide above includes the specific file path and format for rules placement.

#### Rules template

````markdown
# TokToken -- Codebase Index

## Session Init (once per session)

Run `toktoken codebase:detect` at session start.

- exit 0, action "ready" --> use TokToken for code exploration
- exit 0, action "index:create" --> run `toktoken index:create`, then use TokToken
- exit 1 --> not a codebase, do not use TokToken

Cache the result. Do not re-check.

TokToken's smart filter (default: on) excludes CSS, HTML, SVG, TOML, GraphQL, XML, YAML
and vendored subdirectories. If you need to search these, re-index with `--full`.

## Pre-Query Freshness

Before any TokToken query, if you have edited source files since the last
index:update, run `toktoken index:update` first. This uses file-hash comparison
and detects all changes including uncommitted edits.

## Commands

- `toktoken search:symbols "<query>"` -- find functions, classes, methods
- `toktoken search:text "<query>"` -- full-text search (supports pipe OR: "cache|ttl")
- `toktoken inspect:outline "<file>"` -- file structure
- `toktoken inspect:symbol "<id>"` -- retrieve source code for a specific symbol
- `toktoken inspect:bundle "<id>"` -- symbol context bundle (definition + imports + outline)
- `toktoken inspect:file "<file>" --lines START-END` -- file content slice
- `toktoken inspect:tree` -- file tree
- `toktoken find:importers "<file>"` -- find files that import a given file
- `toktoken find:references "<id>"` -- find import references to an identifier
- `toktoken find:callers "<id>"` -- find symbols that likely call a function
- `toktoken search:cooccurrence "<a>,<b>"` -- find symbols co-occurring in same file
- `toktoken search:similar "<id>"` -- find similar symbols by name/summary
- `toktoken inspect:dependencies "<file>"` -- trace transitive import graph
- `toktoken inspect:hierarchy "<file>"` -- show class/function parent-child hierarchy
- `toktoken stats` -- index statistics
- `toktoken index:update` -- refresh after edits

## Key Flags

- `--kind class,method,function` -- filter by symbol type
- `--filter`, `--exclude` -- path filtering (pipe-separated OR)
- `--count` -- count-only mode (useful for negative signals)
- `--group-by file` -- aggregate text search hits per file
- `--compact` -- smaller JSON output (~47% reduction)
- `--limit N` -- cap results
- `--no-sig --no-summary` -- minimal output for discovery queries
- `--context N` / `-C N` -- context lines around matches
- `--debug` -- show per-field score breakdown in search results

## Smart Filter Awareness

TokToken's smart filter (default: on) excludes non-code files and vendored
subdirectories from the index. Excluded extensions: CSS, SCSS, LESS, SASS,
HTML, HTM, SVG, TOML, GraphQL, XML, XUL, YAML, YML.

**When to re-index with `--full`:**

- The user asks about CSS selectors, HTML structure, XML schemas, YAML config,
  OpenAPI specs, GraphQL schemas, TOML config, or SVG content
- A search returns 0 results but the user expects matches in excluded file types
- The user explicitly asks to index "everything" or "all files"
- The project is primarily composed of excluded file types (e.g., a design system
  with mostly CSS/HTML, an infrastructure repo with YAML/HCL, an API project
  with OpenAPI specs)

**How to re-index:**

- MCP: call `index_create` with `full: true` (or `index_update` with `full: true`)
- CLI: `toktoken index:create --full` (or `toktoken index:update --full`)

**Proactive communication:** When you detect that the user's query targets
excluded file types, inform them BEFORE they ask:

> "TokToken's smart filter excludes [CSS/HTML/XML/...] files by default to
> reduce noise. I'll re-index with `--full` to include them."

Do not silently re-index. Explain what was excluded and why you are including it.

## Rules

- Search first, then inspect:symbol for targeted retrieval
- Do not read entire files when a symbol retrieval suffices
- Do not pipe output through jq/python/awk -- use native flags
- Symbol IDs follow the format: `{file}::{qualified_name}#{kind}`

## Update Awareness

When TokToken responses include `update_available` in the `_ttk` metadata,
inform the user once per session: "TokToken update available (current: X.Y.Z,
latest: X.Y.Z). Run `toktoken --self-update` to upgrade."
Do not repeat after first notification. Do not run the update automatically.
````

---

## Step 4: Index the current project

```bash
cd /path/to/project
toktoken codebase:detect
```

If exit code is 0 and the `action` field in the JSON output is `"index:create"`, run:

```bash
toktoken index:create
```

If `action` is `"ready"`, the index already exists.

After editing files, refresh the index before querying:

```bash
toktoken index:update
```

---

## Step 5: Start using TokToken

### Available MCP tools (21 total)

| Tool | Description |
| ---- | ----------- |
| `codebase_detect` | Detect if a directory contains indexable source code |
| `index_create` | Create a full symbol index. Pass `full: true` to disable smart filter |
| `index_update` | Incrementally update the index. Pass `full: true` to disable smart filter |
| `index_github` | Clone and index a GitHub repository |
| `search_symbols` | Search symbols by name (returns IDs for inspect_symbol) |
| `search_text` | Grep-like text search across indexed files |
| `inspect_outline` | Show file symbol outline with line numbers and signatures |
| `inspect_symbol` | Retrieve full source code for symbol(s) by ID |
| `inspect_file` | Retrieve file content, optionally limited to a line range |
| `inspect_bundle` | Get self-contained symbol context: definition source, imports, file outline, optionally importers |
| `inspect_tree` | Show directory tree of indexed source files |
| `stats` | Project statistics: file/symbol counts, staleness, token savings |
| `projects_list` | List all indexed projects |
| `cache_clear` | Delete the index for a project |
| `find_importers` | Find all files that import a given file path |
| `find_references` | Find import statements referencing an identifier or module |
| `find_callers` | Find symbols that likely call a given function/method (heuristic) |
| `search_cooccurrence` | Find symbols that co-occur in the same file |
| `search_similar` | Find symbols similar to a given one by name/summary keywords |
| `inspect_dependencies` | Trace transitive import graph recursively |
| `inspect_hierarchy` | Show class/function hierarchy with parent-child relationships |

### CLI command reference

#### search:symbols

```bash
toktoken search:symbols "auth"
toktoken search:symbols "auth" --kind class,method,function
toktoken search:symbols "auth" --unique --limit 20
toktoken search:symbols "cache" --filter "Frontend|Entity"
toktoken search:symbols "post" --exclude vendor,staging
toktoken search:symbols "ldap" --count
toktoken search:symbols "route" --sort name --kind function --unique
toktoken search:symbols "auth" --no-sig --no-summary
toktoken search:symbols "handler" --language go
```

| Flag | Description |
| ---- | ----------- |
| `--kind <list>` | Comma-separated: class, method, function, property, variable, interface, trait, constant |
| `--unique` | Deduplicate by file:line:name |
| `--sort <field>` | score (default), name, file, line, kind |
| `--count` | Return count only |
| `--no-sig` | Omit signatures |
| `--no-summary` | Omit summaries |
| `--filter <pat>` | Include files matching substring (pipe-separated OR) |
| `--exclude <pat>` | Exclude files matching substring (pipe-separated OR) |
| `--limit <n>` | Cap output results |
| `--max <n>` | Cap index query results (default 50, max 200) |
| `--language <lang>` | Filter by language |
| `--debug` | Show per-field score breakdown (name, signature, summary, keyword, docstring) |
| `--file <glob>` | Filter by file glob |
| `--compact` | Compact JSON output |

#### search:text

```bash
toktoken search:text "TODO"
toktoken search:text "cache|Cache|ttl"
toktoken search:text "author" --group-by file
toktoken search:text "cache" --filter "Frontend|Entity" --group-by file
toktoken search:text "cache" --filter Frontend --count
toktoken search:text "ldap_bind" --context 3
```

| Flag | Description |
| ---- | ----------- |
| `--group-by file` | Hit count per file instead of individual lines |
| `--context <n>` / `-C <n>` | Context lines around each match |
| `--count` | Return count only |
| `--case-sensitive` | Case-sensitive matching |
| `--filter <pat>` | Include files matching substring |
| `--exclude <pat>` | Exclude files matching substring |
| `--limit <n>` | Cap output results |
| `--max <n>` | Cap search results (default 100, max 500) |

#### inspect:outline

```bash
toktoken inspect:outline src/Controller.php
toktoken inspect:outline src/Controller.php --kind class,method
toktoken inspect:outline src/Controller.php --no-sig --no-summary
```

#### inspect:symbol

```bash
toktoken inspect:symbol "src/Auth.php::Auth.login#method"
toktoken inspect:symbol "src/Auth.php::Auth.login#method" --context 5
```

#### inspect:bundle

```bash
toktoken inspect:bundle "src/Auth.php::Auth.login#method"
toktoken inspect:bundle "src/Auth.php::Auth.login#method" --full
toktoken inspect:bundle "src/Auth.php::Auth.login#method" --compact
```

Returns a self-contained context bundle: symbol definition with source code, import lines from the same file, sibling symbols (file outline). Use `--full` to also include importers (files that import this symbol's file).

#### inspect:file

```bash
toktoken inspect:file src/Auth.php
toktoken inspect:file src/Auth.php --lines 10-50
```

#### inspect:tree

```bash
toktoken inspect:tree
toktoken inspect:tree --depth 2
```

#### Other commands

| Command | Description |
| ------- | ----------- |
| `stats` | Index statistics + token savings report |
| `index:create [path]` | Full index from scratch. Use `--full` to include all file types and vendors |
| `index:update [path]` | Incremental re-index. Use `--full` to include all file types and vendors |
| `index:github <owner/repo>` | Clone and index a GitHub repository |
| `inspect:bundle <id>` | Symbol context bundle (definition + imports + outline). Use `--full` for importers |
| `find:importers <file>` | Find files that import a given file |
| `find:references <id>` | Find import statements referencing an identifier |
| `find:callers <id>` | Find symbols that likely call a given function/method |
| `search:cooccurrence "<a>,<b>"` | Find symbols that co-occur in the same file |
| `search:similar <id>` | Find symbols similar to a given one |
| `inspect:dependencies <file>` | Trace transitive import graph. Use `--depth N` (default 3, max 10) |
| `inspect:hierarchy <file>` | Show class/function hierarchy with parent-child relationships |
| `codebase:detect [path]` | Detect if directory is a codebase |
| `projects:list` | List all indexed projects |
| `cache:clear` | Delete the index database |

---

## Workflow Patterns

### Architecture exploration

```text
1. toktoken search:symbols "route" --kind class,method,function --unique --limit 20
2. toktoken search:text "route" --group-by file --exclude vendor
3. toktoken inspect:outline <key-file> --kind class,method
4. Read only the relevant sections of identified files
```

### Negative signal (proving code does NOT exist)

```text
1. toktoken search:text "cache" --filter "Entity" --count
   --> {"count": 0}
   --> Cache is NOT in Entity layer. Skip reading 50+ files.
2. toktoken search:text "cache" --exclude vendor --group-by file
   --> Shows cache is in Dispatcher.php and Api.php
3. Read only those 2 files
```

The `--count` flag is critical for negative signals. It proves a concept does NOT exist in a file set, saving hundreds of lines of unnecessary reading.

### Cross-cutting feature

```text
1. toktoken search:symbols "related_concept" --kind class,method --unique
2. toktoken search:text "related_term" --group-by file --exclude vendor
3. toktoken inspect:outline <each-key-file> --kind method
4. Read only the specific methods/sections to modify
```

### Impact analysis (refactoring scope)

```text
1. toktoken inspect:dependencies "src/auth.py" --depth 3
   --> Shows all files that transitively depend on auth.py
2. toktoken find:callers "src/auth.py::authenticate#function"
   --> Shows which functions call authenticate()
3. toktoken inspect:hierarchy "src/auth.py"
   --> Shows class/method nesting structure
```

### Architectural discovery

```text
1. toktoken search:cooccurrence "Logger,Database"
   --> Which files use both Logger and Database?
2. toktoken search:similar "src/auth.py::validate_token#function"
   --> Find functions similar to validate_token
3. toktoken inspect:hierarchy "src/models.py"
   --> Show class hierarchy with children
```

---

## Configuration

### Project config: `.toktoken.json`

Place in the project root to customize indexing behavior:

```json
{
    "index": {
        "max_file_size_kb": 2048,
        "max_files": 10000,
        "staleness_days": 7,
        "ctags_timeout_seconds": 120,
        "extra_ignore_patterns": ["*.generated.go", "dist/"],
        "languages": ["python", "javascript", "typescript"],
        "extra_extensions": {"blade": "php", "svx": "svelte"},
        "smart_filter": true
    }
}
```

Set `"smart_filter": false` to index all file types including CSS, HTML, and vendored subdirectories.

Global config at `~/.toktoken.json` supports all sections (`index`, `logging`). Project config only supports `index`.

Environment variables (highest priority):

- `TOKTOKEN_EXTRA_IGNORE` -- JSON array or comma-separated patterns
- `TOKTOKEN_STALENESS_DAYS` -- integer (min 1)
- `TOKTOKEN_EXTRA_EXTENSIONS` -- "ext1:lang1,ext2:lang2"
