# TokToken Setup: Gemini CLI

## MCP Server Configuration

Add the following entry to Gemini CLI's settings file. **If the file already exists, read it first, merge the `toktoken` entry into the existing `mcpServers` object, and write it back. Do not overwrite the file.**

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

### Config file locations

Ask the user whether they want global (all projects) or project-scoped (current project only) configuration:

| Scope | Path |
| ----- | ---- |
| **Global** | `~/.gemini/settings.json` |
| **Project** | `.gemini/settings.json` in project root |

Project-scoped configuration takes priority over global.

After editing the config file, restart the Gemini CLI session to load the new MCP server.

## Auto-Approve TokToken Tools (Optional)

Gemini CLI uses a policy engine for granular tool approval. Ask the user if they want to auto-approve all TokToken tools. If yes, create a policy file at `~/.gemini/policies/toktoken.toml`:

```toml
[[rules]]
description = "Auto-approve all TokToken MCP tools"
mcpName = "toktoken"
decision = "allow"
priority = 10
```

This will auto-approve all tools from the `toktoken` MCP server without prompting. See the [Policy Engine documentation](https://geminicli.com/docs/reference/policy-engine/) for more granular control.

## Rules Setup

Append the rules template from [rules-template.md](../rules-template.md) to one of these files:

| Scope | File |
| ----- | ---- |
| **Global** | `~/.gemini/GEMINI.md` |
| **Project** | `GEMINI.md` in project root |

## Official Documentation

- [Gemini CLI MCP](https://github.com/google-gemini/gemini-cli/blob/main/docs/mcp.md)
