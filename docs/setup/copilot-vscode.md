# TokToken Setup: GitHub Copilot in VS Code

> **Important:** VS Code uses a **different JSON format** than most other MCP clients. The top-level key is `"servers"`, **not** `"mcpServers"`.

## MCP Server Configuration

Ask the user whether they want workspace-scoped (shared with the team) or user-scoped (all projects) configuration.

### Workspace-scoped (recommended for teams)

Create or edit `.vscode/mcp.json` in your project root:

```json
{
    "servers": {
        "toktoken": {
            "command": "toktoken",
            "args": ["serve"]
        }
    }
}
```

This file can be committed to version control so all team members get the same MCP configuration.

### User-scoped (all projects)

Open the Command Palette (`Ctrl+Shift+P` / `Cmd+Shift+P`) and search for **"MCP: Add Server"**. Select **"Command (stdio)"**, enter `toktoken` as the server name, and `toktoken serve` as the command.

Alternatively, add the entry manually in your VS Code User Settings JSON (`settings.json`):

```json
{
    "mcp": {
        "servers": {
            "toktoken": {
                "command": "toktoken",
                "args": ["serve"]
            }
        }
    }
}
```

### Verify

After adding the server, open the MCP panel in VS Code to confirm that `toktoken` appears and shows a connected status. You may need to reload the window (`Ctrl+Shift+P` > "Developer: Reload Window").

## Requirements

- VS Code 1.99 or later
- GitHub Copilot extension (active subscription required)
- GitHub Copilot Chat extension

## Official Documentation

- [VS Code MCP Servers](https://code.visualstudio.com/docs/copilot/chat/mcp-servers)
