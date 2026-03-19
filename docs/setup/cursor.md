# TokToken Setup: Cursor

## MCP Server Configuration

Add the following entry to Cursor's MCP configuration file. **If the file already exists, read it first, merge the `toktoken` entry into the existing `mcpServers` object, and write it back. Do not overwrite the file.**

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

Ask the user whether they want project-scoped (shared with the team) or global (all projects) configuration:

| Scope | Path |
| ----- | ---- |
| **Project** | `.cursor/mcp.json` in project root |
| **Global** | `~/.cursor/mcp.json` |

Project-scoped configuration takes priority. The project config file can be committed to version control.

After editing the config file, restart Cursor to load the new MCP server.

## Auto-Approve TokToken Tools (Optional)

By default, Cursor prompts for approval each time an MCP tool is called. Ask the user if they want to auto-approve all TokToken tools. If yes, add `"autoApprove": true` to the `toktoken` server entry:

```json
{
    "mcpServers": {
        "toktoken": {
            "command": "toktoken",
            "args": ["serve"],
            "autoApprove": true
        }
    }
}
```

> **Note:** There are known issues with `autoApprove` in some Cursor versions. If tools still prompt for approval, try enabling "Yolo mode" in Cursor settings as a workaround.

## Rules Setup

Create `.cursor/rules/toktoken.mdc` in your project root with the following YAML frontmatter, followed by the rules template from [rules-template.md](../rules-template.md):

```yaml
---
description: Use TokToken for codebase exploration and symbol retrieval
globs:
---
```

## Official Documentation

- [Cursor MCP](https://docs.cursor.com/context/mcp)
