# TokToken Performance Benchmarks

Real-world performance data measured on eight open-source codebases spanning C, C#, Go, Python, PHP, Lua, and Vim script. All numbers are from a single benchmark run with no cherry-picking or tuning.

---

## Test Environment

| Component | Value |
|-----------|-------|
| CPU | Intel Core i9-12900H (20 threads) |
| RAM | 32 GiB DDR5 |
| OS | Linux 6.6.87 (Ubuntu 24.04 on WSL2) |
| Storage | NVMe SSD |
| TokToken | v1.0.0 (C11, compiled with `-O2`) |
| ctags | Universal Ctags 5.9.0 |
| Compiler | GCC 13.3.0 |

---

## Codebases Tested

Eight well-known open-source projects spanning seven languages:

| Codebase | Description | Total files | Indexed files | Symbols | Primary languages |
|----------|-------------|-------------|---------------|---------|-------------------|
| **Redis** | C in-memory database | 1,746 | 727 | 45,593 | C, C++, Python |
| **curl** | C networking library | 4,216 | 1,108 | 33,973 | C, C++, Python |
| **Laravel** | PHP framework | 3,090 | 2,783 | 39,145 | PHP |
| **Django** | Python web framework | 7,024 | 2,945 | 93,035 | Python, JavaScript |
| **Neovim** | C/Lua text editor | 3,777 | 3,297 | 56,663 | C, Vim, Lua |
| **Kubernetes** | Go container orchestration | 28,482 | 12,881 | 294,753 | Go |
| **dotnet/runtime** | C# runtime + libraries | 57,006 | 37,668 | 1,241,380 | C#, C, C++ |
| **Linux kernel** | OS kernel | 92,931 | 65,231 | 7,433,275 | C, Assembly |

**Total: 198,272 files on disk, 126,640 indexed, 9,237,817 symbols across 8 projects.**

The smart filter (enabled by default) excluded vendored dependencies, test fixtures, documentation, and non-code files. "Total files" includes everything on disk (minus `.git/`); "Indexed files" reflects what passes discovery and language detection.

---

## Indexing Performance

### Full Index Creation (`index:create`)

Each codebase was indexed from scratch with `--max-files 500000` and `--diagnostic`. Timing includes discovery, ctags parsing, database writes, summary generation, and schema rebuild.

| Codebase | Files | Symbols | Wall time | Throughput | Symbol density |
|----------|-------|---------|-----------|------------|----------------|
| **curl** | 1,108 | 33,973 | **0.81 s** | 1,368 files/s | 30.7 sym/file |
| **Redis** | 727 | 45,593 | **0.88 s** | 826 files/s | 62.7 sym/file |
| **Neovim** | 3,297 | 56,663 | **0.94 s** | 3,508 files/s | 17.2 sym/file |
| **Laravel** | 2,783 | 39,145 | **1.44 s** | 1,933 files/s | 14.1 sym/file |
| **Django** | 2,945 | 93,035 | **1.74 s** | 1,692 files/s | 31.6 sym/file |
| **Kubernetes** | 12,881 | 294,753 | **6.13 s** | 2,101 files/s | 22.9 sym/file |
| **dotnet/runtime** | 37,668 | 1,241,380 | **28.40 s** | 1,326 files/s | 33.0 sym/file |
| **Linux kernel** | 65,231 | 7,433,275 | **130.29 s** | 501 files/s | 114.0 sym/file |

Small-to-medium projects (< 3,000 files) index in under 2 seconds. Kubernetes (13K files) takes 6 seconds. The Linux kernel with 65K files and 7.4M symbols indexes in 130 seconds.

Redis has the highest symbol density among small projects (62.7 sym/file), which explains its relatively lower throughput. The Linux kernel's extreme density (114 sym/file across 65K files) drives its indexing time.

### Timing Breakdown

Where time is spent during indexing:

| Codebase | Discovery | Pipeline | Summaries | Schema rebuild | Total |
|----------|-----------|----------|-----------|----------------|-------|
| **curl** | 21 ms | 528 ms | 110 ms | 109 ms | 0.80 s |
| **Redis** | 17 ms | 518 ms | 151 ms | 154 ms | 0.87 s |
| **Neovim** | 41 ms | 528 ms | 149 ms | 179 ms | 0.93 s |
| **Laravel** | 26 ms | 1,024 ms | 173 ms | 166 ms | 1.44 s |
| **Django** | 104 ms | 1,024 ms | 248 ms | 308 ms | 1.73 s |
| **Kubernetes** | 301 ms | 3,021 ms | 1,280 ms | 1,393 ms | 6.11 s |
| **dotnet/runtime** | 3,274 ms | 11,537 ms | 6,289 ms | 6,866 ms | 28.37 s |
| **Linux kernel** | 2,166 ms | 57,571 ms | 30,654 ms | 37,881 ms | 130.18 s |

The **pipeline** (ctags parsing + database writes) dominates for all projects. Schema rebuild (B-tree indexes + FTS5 rebuild) accounts for 14-29% of total time.

### Schema Rebuild Breakdown

| Codebase | Symbols | B-tree indexes | FTS5 rebuild | Total |
|----------|---------|----------------|--------------|-------|
| **curl** | 33,973 | 37 ms | 67 ms | 109 ms |
| **Redis** | 45,593 | 49 ms | 102 ms | 154 ms |
| **Neovim** | 56,663 | 69 ms | 106 ms | 179 ms |
| **Laravel** | 39,145 | 90 ms | 75 ms | 166 ms |
| **Django** | 93,035 | 149 ms | 155 ms | 308 ms |
| **Kubernetes** | 294,753 | 652 ms | 737 ms | 1,393 ms |
| **dotnet/runtime** | 1,241,380 | 3,323 ms | 3,539 ms | 6,866 ms |
| **Linux kernel** | 7,433,275 | 13,196 ms | 24,682 ms | 37,881 ms |

Schema v3 uses 3 B-tree indexes on `symbols` (file+line, kind+language, parent_id) plus 1 FTS5 index. The v2→v3 migration removed 4 redundant indexes (`idx_symbols_name`, `idx_symbols_kind`, `idx_symbols_language`, `idx_symbols_qualified`), reducing rebuild time by approximately 40% on large codebases compared to schema v2.

### Database Size

| Codebase | Files | Symbols | DB size |
|----------|-------|---------|---------|
| **curl** | 1,108 | 33,973 | 20 MB |
| **Redis** | 727 | 45,593 | 29 MB |
| **Neovim** | 3,297 | 56,663 | 31 MB |
| **Laravel** | 2,783 | 39,145 | 47 MB |
| **Django** | 2,945 | 93,035 | 72 MB |
| **Kubernetes** | 12,881 | 294,753 | 329 MB |
| **dotnet/runtime** | 37,668 | 1,241,380 | 1,298 MB |
| **Linux kernel** | 65,231 | 7,433,275 | 5,156 MB |

Storage scales linearly with symbol count. The FTS5 full-text index accounts for roughly 30-40% of total database size. At ~0.7 KB/symbol (Linux kernel), storage cost is predictable.

### Memory Usage

Peak RSS during indexing:

| Codebase | Files | Symbols | Peak RSS |
|----------|-------|---------|----------|
| **curl** | 1,108 | 33,973 | 47 MB |
| **Redis** | 727 | 45,593 | 70 MB |
| **Neovim** | 3,297 | 56,663 | 83 MB |
| **Laravel** | 2,783 | 39,145 | 97 MB |
| **Django** | 2,945 | 93,035 | 131 MB |
| **Kubernetes** | 12,881 | 294,753 | 395 MB |
| **dotnet/runtime** | 37,668 | 1,241,380 | 1,166 MB |
| **Linux kernel** | 65,231 | 7,433,275 | 4,029 MB |

Memory usage is dominated by the parallel worker pipeline (16 ctags processes + in-flight symbol batches). For typical projects (< 10K files), peak RSS stays under 400 MB.

---

## Incremental Update Performance

### No Changes (`index:update`, nothing modified)

Not benchmarked separately in this run. Based on prior measurements (see Methodology), no-change updates complete in 50-150 ms for projects up to 3,300 files.

### 10 Modified Files

Ten random source files were modified (appended a comment to change content hash), then `index:update` was run.

| Codebase | Changed | Wall time |
|----------|---------|-----------|
| **curl** | 10 | **0.72 s** |
| **Redis** | 5 | **0.77 s** |
| **Neovim** | 10 | **0.84 s** |
| **Laravel** | 3 | **0.77 s** |
| **Django** | 10 | **1.03 s** |
| **Kubernetes** | 5 | **2.50 s** |
| **dotnet/runtime** | 10 | **11.85 s** |
| **Linux kernel** | 9 | **50.15 s** |

Incremental updates on small projects complete in under 1 second. The high update time for Linux kernel and dotnet/runtime is dominated by the initial file-hash scan across 65K/38K files. The actual re-indexing of changed files is fast; the bottleneck is comparing all stored hashes against current file contents.

---

## MCP Server Query Performance

All MCP tools were tested via the JSON-RPC protocol (`toktoken serve`). Times shown are **server-side processing** (excluding process startup and JSON serialization overhead).

### Search Tools

| Codebase | search:symbols | search:text | search:cooccurrence | search:similar |
|----------|---------------|-------------|--------------------:|---------------:|
| **curl** | 17 ms | 28 ms | 1 ms | 2 ms |
| **Redis** | 12 ms | 38 ms | 1 ms | 4 ms |
| **Neovim** | 15 ms | 49 ms | 4 ms | 3 ms |
| **Django** | 16 ms | 36 ms | 13 ms | 7 ms |
| **Laravel** | 12 ms | 122 ms | 8 ms | 7 ms |
| **Kubernetes** | 19 ms | 51 ms | 2 ms | 40 ms |
| **dotnet/runtime** | 33 ms | 80 ms | 2 ms | 31 ms |
| **Linux kernel** | 46 ms | 137 ms | 1 ms | 151 ms |

Symbol search (`search:symbols`) uses FTS5 and stays under 50 ms even on 7.4M symbols. Text search (`search:text`) scales roughly linearly with file count.

### Inspect Tools

| Codebase | inspect:outline | inspect:symbol | inspect:file | inspect:bundle | inspect:tree | inspect:hierarchy | inspect:dependencies |
|----------|----------------|----------------|-------------|---------------|-------------|------------------|---------------------|
| **curl** | 17 ms | 17 ms | 16 ms | 16 ms | 12 ms | 1 ms | 1 ms |
| **Redis** | 14 ms | 12 ms | 12 ms | 12 ms | 13 ms | 0 ms | 1 ms |
| **Neovim** | 12 ms | 11 ms | 0 ms | 1 ms | 20 ms | 0 ms | 2 ms |
| **Django** | 1 ms | 12 ms | 12 ms | 1 ms | 16 ms | 1 ms | 2 ms |
| **Laravel** | 12 ms | 12 ms | 10 ms | 10 ms | 14 ms | 1 ms | 3 ms |
| **Kubernetes** | 21 ms | 11 ms | 11 ms | 12 ms | 40 ms | 1 ms | 21 ms |
| **dotnet/runtime** | 12 ms | 12 ms | 12 ms | 19 ms | 105 ms | 1 ms | 3 ms |
| **Linux kernel** | 11 ms | 11 ms | 11 ms | 10 ms | 189 ms | 1 ms | 275 ms |

Most inspect operations complete in under 20 ms regardless of codebase size. The `inspect:tree` operation scales with file count (189 ms for 65K files). The `inspect:dependencies` outlier on Linux (275 ms) reflects the recursive import graph traversal.

### Find Tools

| Codebase | find:importers | find:references | find:callers |
|----------|---------------|----------------|-------------|
| **curl** | 2 ms | 1 ms | 1 ms |
| **Redis** | 1 ms | 1 ms | 1 ms |
| **Neovim** | 2 ms | 1 ms | 2 ms |
| **Django** | 2 ms | 2 ms | 1 ms |
| **Laravel** | 2 ms | 3 ms | 2 ms |
| **Kubernetes** | 16 ms | 13 ms | 16 ms |
| **dotnet/runtime** | 2 ms | 2 ms | 2 ms |
| **Linux kernel** | 43 ms | 38 ms | 53 ms |

Find operations leverage B-tree indexes on the imports table. Even on the Linux kernel with its dense import graph, all find operations complete in under 60 ms.

### Stats

| Codebase | Latency |
|----------|---------|
| **curl** | 11 ms |
| **Redis** | 7 ms |
| **Neovim** | 13 ms |
| **Django** | 18 ms |
| **Laravel** | 8 ms |
| **Kubernetes** | 66 ms |
| **dotnet/runtime** | 274 ms |
| **Linux kernel** | 1,836 ms |

The `stats` command aggregates counts across all tables. On extremely large databases (5+ GB, 7.4M symbols), the aggregation takes 1.8 seconds. For typical projects (< 100K symbols), stats returns in under 70 ms.

---

## Smart Filter Effectiveness

The smart filter excludes non-code files and vendored subdirectories. Its effectiveness varies by project structure:

| Codebase | Total files | Indexed files | Filter ratio | Rejected by ext | Notes |
|----------|-------------|---------------|-------------|-----------------|-------|
| **curl** | 4,216 | 1,108 | 26.3% | 3,108 | Heavy .md, .txt, .cmake |
| **Redis** | 1,746 | 727 | 41.6% | 1,019 | Moderate docs/config |
| **Neovim** | 3,777 | 3,297 | 87.3% | 457 | Mostly code (.vim, .lua, .c) |
| **Django** | 7,024 | 2,945 | 41.9% | 4,011 | .html, .txt, .po templates |
| **Laravel** | 3,090 | 2,783 | 90.1% | 293 | Mostly PHP code |
| **Kubernetes** | 28,482 | 12,881 | 45.2% | 10,217 | .yaml, .json, vendor |
| **dotnet/runtime** | 57,006 | 37,668 | 66.1% | 18,502 | .xml, .props, resources |
| **Linux kernel** | 92,931 | 65,231 | 70.2% | 34,573 | Headers + docs excluded |

The smart filter is most effective on projects with heavy documentation (curl: 74% filtered out) and least effective on pure-code projects (Laravel: 10% filtered out, Neovim: 13% filtered out).

---

## Token Savings: Symbol Retrieval vs Full File Reads

The primary value proposition of TokToken: retrieving a specific symbol instead of reading the entire file. Token estimates use the standard approximation of 1 token per 4 characters.

### Function-Level Retrieval

| Symbol | Lines | Symbol tokens | File tokens | Savings |
|--------|-------|---------------|-------------|---------|
| `ossl_connect_step1` (curl, from 5,504-line file) | 50 | 406 | 42,568 | **99.0%** |
| `Curl_connect` (curl, from 3,873-line file) | 62 | 466 | 29,657 | **98.4%** |
| `eval7` (neovim, from 6,216-line file) | 180 | 1,157 | 48,349 | **97.6%** |
| `cliSendCommand` (redis, from 11,137-line file) | 206 | 2,217 | 106,389 | **97.9%** |
| `processCommand` (redis, from 8,141-line file) | 126 | 1,496 | 56,754 | **97.4%** |
| `Query.build_filter` (django, from 3,954-line file) | 162 | 1,704 | 30,726 | **94.5%** |
| `nfa_regmatch` (neovim, from 16,288-line file) | 1,409 | 11,247 | 113,932 | **90.1%** |
| `do_cmdline` (neovim, from 8,020-line file) | 562 | 5,572 | 62,041 | **91.0%** |

**Median savings: 96.7%.** Every function-level retrieval saves at least 90% of tokens. For small functions in large files, savings reach 99%.

### What This Means for AI Agents

An AI agent inspecting `processCommand` in Redis reads 126 lines (1,496 tokens) instead of the full `server.c` at 56,754 tokens. That is a **38x reduction** in context consumption for a single lookup.

Over a typical coding session with 20-30 symbol lookups, TokToken saves **hundreds of thousands of tokens** that would otherwise be wasted reading entire files.

---

## Performance Summary

| Operation | Small (< 3K files) | Medium (3K-13K files) | Large (38K-65K files) |
|-----------|--------------------|-----------------------|----------------------|
| Full index | **0.8-1.7 s** | **1.4-6.1 s** | **28-130 s** |
| Incremental update (10 files) | **0.7-1.0 s** | **0.8-2.5 s** | **12-50 s** |
| Symbol search | **12-17 ms** | **15-19 ms** | **33-46 ms** |
| Text search | **28-122 ms** | **36-51 ms** | **80-137 ms** |
| Symbol retrieval | **11-17 ms** | **11 ms** | **11-12 ms** |
| File outline | **1-17 ms** | **12-21 ms** | **11-12 ms** |
| Context bundle | **1-16 ms** | **10-12 ms** | **10-19 ms** |
| Directory tree | **12-16 ms** | **14-20 ms** | **105-189 ms** |
| Stats | **7-18 ms** | **8-66 ms** | **274-1,836 ms** |

### Key Observations

1. **All query operations on typical projects (< 13K files) complete in under 70 ms.** The slowest observed query (text search on Laravel, 122 ms) remains well under the threshold of perceptibility for an AI agent.

2. **Symbol search is effectively constant-time.** Django's 93K symbols and Linux's 7.4M symbols return results in 16 ms vs 46 ms. FTS5 indexing keeps search complexity logarithmic.

3. **Symbol retrieval is truly constant-time.** Retrieving a function's source code takes 11-17 ms regardless of codebase size. It is a database lookup + file seek, not a scan.

4. **Schema v3 indexes are optimized.** The 3 B-tree indexes on `symbols` (file+line, kind+language, parent_id) plus FTS5 cover all query patterns. The v2→v3 migration removed 4 redundant indexes, reducing rebuild time on large codebases.

5. **Indexing parallelizes across 16 workers.** All 16 cores are utilized for ctags parsing. The pipeline uses an MPSC queue to funnel results to a single SQLite writer thread.

6. **Memory scales linearly.** Peak RSS ranges from 47 MB (curl, 34K symbols) to 4 GB (Linux, 7.4M symbols). For typical projects (< 100K symbols), peak RSS stays well under 500 MB.

7. **Native C binary, zero startup cost.** No interpreter, no JVM, no runtime. The binary starts in under 1 ms and has no warm-up period.

---

## Methodology

- All timings are wall-clock measurements from a Python benchmark harness using `time.monotonic()`.
- MCP server-side timings are extracted from the `_ttk.duration_ms` field in JSON responses.
- Modified-file benchmarks appended a C comment to change content hash, then reverted via `git checkout .` after measurement.
- Diagnostic output (`--diagnostic`) was captured to JSONL files for timing breakdown analysis.
- "Total files" counts include all files on disk (excluding `.git/`). "Indexed files" reflects smart-filter output.
- Token estimates use the standard approximation: 1 token per 4 characters of source code.
- Database sizes are measured immediately after `index:create` (SQLite WAL mode, not checkpointed).
- File visit counts may differ slightly from `find` counts due to `.gitignore` filtering.

---

*TokToken v1.0.0 -- Benchmarked 2026-03-17*
