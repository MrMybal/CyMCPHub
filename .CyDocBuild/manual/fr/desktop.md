---
id: desktop
title: Arrière-plan et données
description: Garder le Hub accessible et retrouver les annotations et Skills.
section: Utiliser CyMCPHub
order: 3
revision: 3
sourceRevision: 3
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

## Installer ou mettre à jour CyMCPHub {#updates}

Sous Windows x64, lancez le programme d’installation depuis les [releases GitHub](https://github.com/MrMybal/CyMCPHub/releases). Il installe l’app pour votre compte, crée un raccourci dans le menu Démarrer et ajoute une entrée dans **Applications installées**.

1. Ouvrez **Mises à jour → Rechercher une mise à jour**. Le Hub consulte la dernière release stable sur GitHub et la compare à la version en cours.
2. Si un installateur Windows compatible est proposé, choisissez **Télécharger l’installateur**. Le Hub vérifie sa taille et son SHA-256 à partir des métadonnées du fichier GitHub.
3. Choisissez **Quitter et installer**, puis terminez l’assistant. L’installateur réutilise le dossier d’une installation existante et propose de relancer l’app. Vous pouvez passer d’une copie portable à une installation par utilisateur avec le même assistant. Les annotations et les Skills continuent d’utiliser le même [dossier de données](doc:desktop#data).

**Voir la release** ouvre la page GitHub correspondante. Utilisez-la pour les autres plateformes et les releases fournies sous forme d’archive.

## Licence de l’application {#license}

CyMCPHub est distribué sous la licence publique générale GNU, version 3 uniquement (`GPL-3.0-only`). Le fichier `LICENSE` à la racine de chaque paquet contient le texte de la licence. Le paquet contient aussi les notices des dépendances.
