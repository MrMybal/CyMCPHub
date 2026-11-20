---
id: desktop
title: Arrière-plan et données
description: Garder le Hub accessible et retrouver les annotations et Skills.
section: Utiliser CyMCPHub
order: 3
revision: 1
sourceRevision: 1
parent: welcome
---

## Garder le Hub accessible {#background}

Quand l’icône de la zone de notification est disponible, fermer la fenêtre principale laisse le Hub actif. Le menu de l’icône permet de rouvrir la fenêtre principale ou l’assistant, d’activer le démarrage à l’ouverture de session et de quitter l’application. `--tray` démarre avec la fenêtre principale masquée. Sous Ubuntu, l’affichage de l’icône nécessite un bureau qui prend en charge AppIndicator et le paquet `libayatana-appindicator3-1`.

## Retrouver le dossier de données {#data}

| Système | Par défaut |
| --- | --- |
| Windows | `%LOCALAPPDATA%\CyMCPHub` |
| Linux | `$XDG_DATA_HOME/cymcphub` ou `~/.local/share/cymcphub` |
| macOS | `~/Library/Application Support/CyMCPHub` |

La fenêtre et le serveur stdio partagent les annotations et les Skills de ce dossier. Définissez `CYMCP_HUB_DATA` pour utiliser un autre dossier. Le journal est conservé en mémoire pour le processus courant.
