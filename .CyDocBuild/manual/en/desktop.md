---
id: desktop
title: Background service and data
description: Keep the Hub available and locate its local annotations and Skills.
section: Using CyMCPHub
order: 3
revision: 2
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

## Application licence {#license}

CyMCPHub is distributed under the GNU General Public License, version 3 only (`GPL-3.0-only`). The `LICENSE` file at the root of each package contains the terms. The package also includes the individual notices for its dependencies.
