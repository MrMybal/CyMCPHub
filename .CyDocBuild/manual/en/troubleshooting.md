---
id: troubleshooting
title: Troubleshoot MCP connections
description: Find a missing connector, check transport errors and inspect the journal.
section: Using CyMCPHub
order: 5
revision: 1
parent: welcome
---

## A connector is missing {#inventory}

1. Clear the search and the **Active groups only** filter in **MCP servers**.
2. In **Sources**, select the connector's project and check that its JSON, JSONC or TOML configuration is selected. Enable **Probe local TCP ports** to look for loopback listeners.
3. Click **Refresh** and open the group. Each connector has its own source, transport and status. A configured stdio entry can be opened from **Tools** even when the original process is stopped.

The standalone MCP executable can print an inventory. Run it from the unpacked directory on Windows.

```powershell
.\CyMCPHubMcp.exe --workspace . --no-port-scan --list
```

On Linux, run `./CyMCPHubMcp --workspace . --no-port-scan --list`. Add `--config <file>` to include a separate configuration. Drop `--no-port-scan` to include discovered TCP listeners.

## The connector opens but tools fail to load {#tools}

1. Select the intended connector and open **Connection**. Check the transport and its source file. For HTTP, confirm the path and bring the server up before repeating the request.
2. For HTTP 401 or 403, check that the configuration supplies the connector's expected Bearer authentication. If it names an environment variable, set it in the environment inherited by the Hub and reopen the Hub.
3. If the journal reports invalid JSON or an HTTP request rejection, inspect the reported request method and status. Check that the URL is the server's Streamable HTTP endpoint and that it accepts MCP initialization. After correcting the source configuration, use **Disconnect**, refresh the inventory and load the tools again.

Open **Journal** in the desktop Hub to inspect its operations. When diagnosing a request sent through a separate MCP process, call `mcp_journal` through that same connection. Each journal covers its own process.

## A client cannot reach the Hub {#client}

For stdio, confirm that the copied configuration points to the current `CyMCPHubMcp` executable and that the client can launch it. For HTTP, confirm that the Hub is running, **MCP service (HTTP)** is active and the client uses the configuration copied after activation. [Connection and tools](doc:mcp#connect) explains both modes.
