<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="Documentation/Documentation.html#/en/welcome">Offline manual</a><br>
      <a href="https://github.com/MrMybal/CyMCPHub">Repository</a><br>
      <a href="https://github.com/MrMybal/CyMCPHub/issues">Report an issue</a><br><br>
      <sub>README</sub><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="Documentation/DocData/assets/cymcphub-logo.png" alt="CyMCPHub logo" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Application licence</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>Version</sub><br><a href="VERSION.md"><strong>0.3.1</strong></a></p>
      <p><strong>Windows x64 · Linux x64</strong><br><sub>macOS build target</sub></p>
    </td>
  </tr>
</table>

<h1 align="center">CyMCPHub</h1>

A desktop explorer for local MCP servers, SDKs and AI CLIs, with a searchable tool catalogue and contextual assistant. Use the Hub to find connections and share the catalogue with another AI client.

Read the [version notes](VERSION.md) for platform coverage and current limits.

[![CyMCPHub showing the local inventory, SDK, CLI, Skills and Journal tabs](Documentation/DocData/assets/images/hub-main.png)](Documentation/DocData/assets/images/hub-main.png)

## What you can do

- **Inspect a connector.** Group servers by name, select a source and transport, read its connection details and add an annotation. Load its tools and resources, run an action with JSON arguments and check the journal.
- **Scan local software.** Select a file or directory to find CyMCPSDK markers and known CLIs. Collect any `SKILL.md` files found in scans or exposed by a connector.
- **Work in the assistant.** Open the chat in a separate window that follows the active tab. Connect a supported provider, attach a connector and draft a Skill for it.
- **Share the catalogue.** Connect a client through stdio or local Streamable HTTP. `mcp_catalog` searches the applications and their tools and can return Markdown.

---

## Run CyMCPHub

| Platform | Launch from the native package |
| --- | --- |
| Windows x64 | Run the installer or open `CyMCPHub.exe` from a portable build. Keep `CyMCPHubMcp.exe` beside it in portable mode. Builds linked to the shared Microsoft Visual C++ runtime require the x64 redistributable. |
| Ubuntu 24.04 x64 | Install `libcurl4t64`, then run `./CyMCPHub` beside `CyMCPHubMcp`. The tray requires an AppIndicator-compatible desktop and `libayatana-appindicator3-1`. |
| macOS 14+ · Intel and Apple Silicon | The universal `CyMCPHub.app` target is available in the build workflow. A macOS build and desktop validation are pending. |

Open [Documentation.html](Documentation/Documentation.html#/en/welcome) from the unpacked project or package in a browser. Keep the `DocData` folder alongside it. **AI setup** in the Hub generates the client configuration and enables the background HTTP service.

The Windows installer installs per user and adds a Start menu shortcut. Use **Updates → Check for updates** in the app to check the latest stable GitHub release. On Windows x64, when a matching installer is available, download it and choose **Quit and install**. The Hub verifies the file size and SHA-256 supplied by GitHub before launching Setup. [Offline update guide](Documentation/Documentation.html#/en/desktop/updates).

## Connect an AI client

Use the **Settings** page in the sidebar to choose inventory sources, set the refresh interval, and copy AI client configuration. The MCP inventory refreshes every minute by default, including when the window is hidden in the tray. The local HTTP service has a configurable port and supports token-protected or free local access. The service's **Start with the app** option and **Start at login** are independent.

1. In **Sources**, select the project and add its MCP configuration if needed.
2. Open **AI setup**. Choose **stdio** to let your client launch the Hub's MCP server, or enable **MCP service (HTTP)** to use the running desktop Hub.
3. Copy the configuration into your client. Call `mcp_list` to find a connector ID, then call `mcp_tools` with that ID to see the tool schemas. Use `mcp_catalog` with `format: "markdown"` for a summary across applications.

The offline manual includes a [worked MCP example](Documentation/Documentation.html#/en/mcp/tools-example) and [troubleshooting steps](Documentation/Documentation.html#/en/troubleshooting/inventory) for missing connectors and errors while loading tools.

## Build from source

See the [local build and packaging commands](BUILD.md). The toolchain uses CMake 3.24+, a C++20 compiler, Git and Python 3 for the integration tests. Node.js 22+ regenerates the manual. The repository also includes a [manual packaging workflow](https://github.com/MrMybal/CyMCPHub/blob/main/.github/workflows/native-packages.yml).

---

## Licence

CyMCPHub is licensed under the [GNU General Public License, version 3 only](LICENSE) (`GPL-3.0-only`).

### Dependency licences

The native packages include the notices for their bundled components.

| Component | Licence | Purpose |
| --- | --- | --- |
| [CyUI](https://github.com/MrMybal/CyUI) and Dear ImGui | MIT | Interface and native widgets |
| [CyMCPSDK](https://github.com/MrMybal/CyMCPSDK), nlohmann/json, cpp-httplib, PicoSHA2 and toml++ | MIT | MCP, assistant, JSON, HTTP and configuration |
| MD4C | MIT | Markdown rendering |
| Lexilla and Scintilla | [Lexilla/Scintilla licence](https://www.scintilla.org/License.txt) | Text and code views |
| JetBrains Mono | SIL Open Font License 1.1 | Interface font |
| [SDL3](https://github.com/libsdl-org/SDL) | zlib | Linux and macOS windows, renderer and tray |
| [CyDoc](https://github.com/MrMybal/CyDoc) | [MIT](Documentation/DocData/CyDoc.LICENSE.txt) | Documentation generator and offline reader |
