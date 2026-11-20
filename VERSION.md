# CyMCPHub 0.3.0

- Inventory of local HTTP and configured stdio MCP servers, grouped by connector name.
- SDK and CLI scanning, annotations, journal and a shared `SKILL.md` library.
- Separate AI chat window with the active tab and selected item as context.
- Local MCP service with a searchable catalogue and Markdown output.
- Windows x64 and Ubuntu 24.04 x64 builds. The native package workflow also targets macOS 14+ on Intel and Apple Silicon. That target still needs a build and desktop validation on macOS.

Linux tray availability depends on the desktop’s AppIndicator support. The inventory only sees processes and sockets accessible to the current user. MCP prompts and the legacy SSE transport are outside the connector interface.
