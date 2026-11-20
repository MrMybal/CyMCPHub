---
id: mcp
title: Connexion et outils MCP
description: Connecter un client IA en stdio ou en HTTP Streamable local.
section: Utiliser CyMCPHub
order: 4
revision: 2
sourceRevision: 2
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

## Faire une première requête {#tools-example}

1. Appelez `mcp_list` avec `{"query":"notes","limit":10,"refresh":true}` et retrouvez l’entrée concernée dans `servers`. Omettez `query` pour afficher tous les connecteurs. Lorsque le résultat contient `nextOffset`, passez sa valeur dans `offset` pour obtenir la page suivante.
2. Appelez `mcp_get` avec l’`id` de l’entrée pour confirmer sa source et son transport. Appelez `mcp_tools` avec le même `id` pour consulter les schémas. Le chargement des outils d’un connecteur stdio lance une session dédiée.
3. Appelez `mcp_catalog` avec `{"format":"markdown","toolLimit":10}` pour obtenir une synthèse. Par défaut, le catalogue interroge les connecteurs HTTP confirmés. Indiquez l’identifiant d’un connecteur pour inclure ses outils stdio si nécessaire.

Si la liste ou les schémas manquent, consultez [Dépanner les connexions MCP](doc:troubleshooting#inventory).
