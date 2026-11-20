---
id: mcp
title: MCP connection and tools
description: Connect another AI client over stdio or local Streamable HTTP.
section: Using CyMCPHub
order: 4
revision: 1
parent: welcome
---

## Connect an AI client {#connect}

In **AI setup**, copy the generated `mcpServers` configuration into your client. In stdio mode, the client starts `CyMCPHubMcp` as needed. The macOS target places that binary inside `CyMCPHub.app/Contents/MacOS/`.

For a background connection, open **AI setup → MCP service (HTTP)**, enable the service and copy its configuration. It listens on `http://127.0.0.1:48786/mcp`. The copied configuration contains a bearer token. Keep it in the client's private configuration. The service starts again with the Hub after activation.

Both executables accept `--workspace <directory>` and `--config <file>`. Use `--no-port-scan` to disable TCP discovery. The server also supports `--list` to print a JSON snapshot and exit.

## Public tools {#tools}

| Tool | What it does |
| --- | --- |
| `mcp_list` | List and filter connectors. Use `refresh: true` to rescan, and `groupKey` to identify connectors that share a name. |
| `mcp_catalog` | Search applications and tools. Set `format: "markdown"` to get an overview. Use `id` or `includeStdio` to request stdio catalogue access. |
| `mcp_get` | Read the source, connection metadata and annotation by connector ID. |
| `mcp_annotate` | Replace an annotation. Submit an empty string to clear it. |
| `mcp_tools` and `mcp_call` | Get a connector’s input schemas and run a selected tool with arguments. |
| `mcp_resources` and `mcp_read_resource` | List and read resources on the selected connector. |
| `mcp_disconnect` | Close the session established by the Hub. |
| `mcp_journal` | Retrieve the current process journal in JSON Lines. |
| `mcp_list_skills` and `mcp_read_skill` | Find and read Skills in the Hub library. |
| `mcp_collect_skills` and `mcp_import_skill` | Collect Skills from a connector or import a file from the workspace. |
| `mcp_save_skill` | Validate and save a new Skill for the chosen connector. |
| `sdk_scan` | Scan software by a workspace-relative `path`. Set `includePath: false` to limit results to that target. |

For a searchable overview, call `mcp_catalog` with `{"query":"search","format":"markdown"}`. Use `mcp_tools` on the connector ID for the full input schemas.
