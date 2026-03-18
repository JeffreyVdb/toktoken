# TokToken Setup: OpenAI Codex CLI

> **Note:** Codex CLI does not natively support MCP servers. Integration is via rules files (AGENTS.md) and CLI commands.

## Rules Setup

Ask the user whether they want global (all projects) or project-scoped (current project only) configuration.

Append the rules template from [LLM.md](../LLM.md#rules-template) to one of these files:

| Scope | File |
| ----- | ---- |
| **Global** | `~/.codex/AGENTS.md` |
| **Project** | `AGENTS.md` in project root |

Codex CLI will automatically read AGENTS.md and use TokToken via shell commands (`toktoken search:symbols`, `toktoken inspect:symbol`, etc.).

## Official Documentation

- [Codex CLI AGENTS.md](https://github.com/openai/codex/blob/main/docs/AGENTS.md)
