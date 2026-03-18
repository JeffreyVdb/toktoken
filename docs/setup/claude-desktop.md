# TokToken Setup: Claude Desktop

## MCP Server Configuration

Add the following entry to your Claude Desktop configuration file. **If the file already exists, read it first, merge the `toktoken` entry into the existing `mcpServers` object, and write it back. Do not overwrite the file.**

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

| OS | Path |
| -- | ---- |
| **macOS** | `~/Library/Application Support/Claude/claude_desktop_config.json` |
| **Windows** | `%APPDATA%\Claude\claude_desktop_config.json` |

You can also reach the config file from within Claude Desktop: **Settings > Developer > Edit Config**.

> **Note:** Claude Desktop for Linux is not officially supported at the time of writing. Check the official documentation for updates.

After editing the config file, restart Claude Desktop to load the new MCP server.

## Official Documentation

- [MCP Quickstart for Users](https://modelcontextprotocol.io/quickstart/user)
