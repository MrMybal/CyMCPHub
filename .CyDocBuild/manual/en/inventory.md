---
id: inventory
title: Explore the inventory
description: Find local MCP connectors, inspect their tools and scan installed software.
section: Using CyMCPHub
order: 1
revision: 1
parent: welcome
---

## Servers and connectors {#connectors}

**MCP servers** groups connectors that share a name. Each source, transport and annotation stays attached to its own connector. Use the search and active-groups filter to narrow the list.

**Verified MCP** means a local HTTP endpoint answered MCP initialization. TCP discovery checks `/mcp` on local loopback listeners. A configured local HTTP path can also be verified. **Process observed** means the command of a configured stdio connector matches a readable local process. Authentication and process visibility can affect these results.

The Hub reads common settings for Codex, Claude Desktop, Claude Code, VS Code, Cursor, Gemini CLI, Windsurf and OpenCode. Connection details mask values of configured credentials. Resolve them locally through their source config.

## Call tools and read resources {#actions}

Open **Tools**, load the selected connector's list and choose an action. Review its input schema, enter JSON arguments and run it. Use **Resources** to list and read URIs. A stdio connection launches a dedicated process. **Disconnect** closes the selected session. The **Journal** reports operations, HTTP statuses and assistant diagnostics. Copy it as JSON Lines when you need to share a diagnostic.

## Search SDKs and CLIs {#software}

In **SDK** or **CLI**, choose a file or folder. The Hub looks for CyMCPSDK build references, library names, binary markers and known CLI names. It reads files without launching them. The CLI inventory can include executables detected on the `PATH`. Each result shows its path and the evidence for the detection.
