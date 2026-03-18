# TokToken Setup: Gemini Code Assist (VS Code)

> **Note:** Gemini Code Assist is a separate product from Gemini CLI. It runs as a VS Code extension and uses a different configuration path.

## MCP Server Configuration

Create or edit `.gemini/settings.json` **in your project root** (not the global `~/.gemini/settings.json`):

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

After editing the config file, reload VS Code to load the new MCP server.

## Official Documentation

- [Gemini Code Assist MCP Servers](https://developers.google.com/gemini-code-assist/docs/use-mcp-servers)
