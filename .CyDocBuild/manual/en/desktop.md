---
id: desktop
title: Background service and data
description: Keep the Hub available and locate its local annotations and Skills.
section: Using CyMCPHub
order: 3
revision: 3
parent: welcome
---

## Keep the Hub available {#background}

When the tray icon is available, closing the main window keeps the Hub running. The tray menu reopens the main or assistant window, enables startup at sign-in and quits the app. `--tray` starts with the main window hidden. Ubuntu desktops need an AppIndicator host and `libayatana-appindicator3-1` to show the icon.

## Find the data folder {#data}

| Platform | Default |
| --- | --- |
| Windows | `%LOCALAPPDATA%\CyMCPHub` |
| Linux | `$XDG_DATA_HOME/cymcphub` or `~/.local/share/cymcphub` |
| macOS | `~/Library/Application Support/CyMCPHub` |

The desktop window and stdio server share annotations and Skills in this directory. Set `CYMCP_HUB_DATA` to use another folder. The journal is kept in memory for the current process.

## Install or update CyMCPHub {#updates}

On Windows x64, run the setup program from the [GitHub releases](https://github.com/MrMybal/CyMCPHub/releases). Setup installs the app for your user account, adds a Start menu shortcut and registers it in **Installed apps**.

1. Open **Updates → Check for updates**. The Hub queries the latest stable GitHub release and compares it with the running version.
2. When a matching Windows installer is listed, choose **Download installer**. The Hub checks its size and SHA-256 against the GitHub asset metadata.
3. Choose **Quit and install** and finish the setup wizard. Setup reuses an existing installation folder and offers to launch the app again. You can replace a portable copy with a per-user installation using the same wizard. Annotations and Skills keep using the same [data folder](doc:desktop#data).

**View release** opens the corresponding GitHub page. Use it for other platforms and releases that only provide an archive.

## Application licence {#license}

CyMCPHub is distributed under the GNU General Public License, version 3 only (`GPL-3.0-only`). The `LICENSE` file at the root of each package contains the terms. The package also includes the individual notices for its dependencies.
