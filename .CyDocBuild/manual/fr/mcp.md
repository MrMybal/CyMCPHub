---
id: mcp
title: Connexion et outils MCP
description: Connecter un client IA en stdio ou en HTTP Streamable local.
section: Utiliser CyMCPHub
order: 4
revision: 1
sourceRevision: 1
parent: welcome
---

## Connecter un client IA {#connect}

Dans **Connexion IA**, copiez la configuration `mcpServers` générée vers votre client. En stdio, le client lance `CyMCPHubMcp` à la demande. La cible macOS place ce binaire dans `CyMCPHub.app/Contents/MacOS/`.

Pour une connexion en arrière-plan, ouvrez **Connexion IA → Service MCP (HTTP)**, activez-le et copiez sa configuration. Il écoute sur `http://127.0.0.1:48786/mcp`. La configuration copiée contient un jeton Bearer. Conservez-le dans la configuration privée du client. Le service redémarre avec le Hub après activation.

Les deux exécutables acceptent `--workspace <dossier>` et `--config <fichier>`. Utilisez `--no-port-scan` pour désactiver la découverte TCP. Le serveur accepte aussi `--list` pour afficher l’inventaire en JSON puis quitter.

## Outils publics {#tools}

| Outil | Opération |
| --- | --- |
| `mcp_list` | Lister et filtrer les connecteurs. Utilisez `refresh: true` pour actualiser et `groupKey` pour retrouver les connecteurs qui partagent un nom. |
| `mcp_catalog` | Rechercher des applications et leurs outils. Choisissez `format: "markdown"` pour obtenir une synthèse. Utilisez `id` ou `includeStdio` pour demander l’accès à un catalogue stdio. |
| `mcp_get` | Lire la source, les données de connexion et l’annotation à partir de l’identifiant du connecteur. |
| `mcp_annotate` | Remplacer une annotation. Fournissez une chaîne vide pour l’effacer. |
| `mcp_tools` et `mcp_call` | Obtenir les schémas des actions d’un connecteur et exécuter l’action choisie avec ses arguments. |
| `mcp_resources` et `mcp_read_resource` | Lister et lire les ressources du connecteur sélectionné. |
| `mcp_disconnect` | Fermer la session établie par le Hub. |
| `mcp_journal` | Récupérer le journal du processus courant en JSON Lines. |
| `mcp_list_skills` et `mcp_read_skill` | Rechercher et lire les Skills enregistrés dans le Hub. |
| `mcp_collect_skills` et `mcp_import_skill` | Collecter les Skills d’un connecteur ou importer un fichier du projet. |
| `mcp_save_skill` | Valider et enregistrer un nouveau Skill pour le connecteur choisi. |
| `sdk_scan` | Analyser un logiciel via un `path` relatif au projet. Utilisez `includePath: false` pour limiter les résultats à cette cible. |

Pour une synthèse filtrée, appelez `mcp_catalog` avec `{"query":"search","format":"markdown"}`. Utilisez `mcp_tools` avec l’identifiant du connecteur pour obtenir les schémas complets.
