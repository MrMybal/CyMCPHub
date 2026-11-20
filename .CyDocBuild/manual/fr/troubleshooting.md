---
id: troubleshooting
title: Dépanner les connexions MCP
description: Retrouver un connecteur, vérifier les erreurs de transport et consulter le journal.
section: Utiliser CyMCPHub
order: 5
revision: 1
sourceRevision: 1
parent: welcome
---

## Un connecteur manque dans l’inventaire {#inventory}

1. Effacez la recherche et désactivez **Groupes actifs uniquement** dans **Serveurs MCP**.
2. Dans **Sources**, sélectionnez le projet du connecteur et vérifiez que son fichier JSON, JSONC ou TOML est pris en compte. Activez **Rechercher sur les ports TCP locaux** pour détecter les services en écoute sur la boucle locale.
3. Cliquez sur **Actualiser** et ouvrez le groupe. Chaque connecteur possède sa propre source, son transport et son état. Vous pouvez ouvrir une entrée stdio configurée depuis **Outils**, même si le processus d’origine est arrêté.

L’exécutable MCP peut afficher l’inventaire. Sous Windows, lancez-le depuis le dossier extrait.

```powershell
.\CyMCPHubMcp.exe --workspace . --no-port-scan --list
```

Sous Linux, lancez `./CyMCPHubMcp --workspace . --no-port-scan --list`. Ajoutez `--config <fichier>` pour inclure une configuration séparée. Retirez `--no-port-scan` pour inclure les ports TCP détectés.

## Le connecteur s’ouvre mais ses outils restent indisponibles {#tools}

1. Sélectionnez le connecteur voulu et ouvrez **Connexion**. Vérifiez son transport et son fichier source. En HTTP, confirmez le chemin et démarrez le serveur avant de réessayer.
2. Pour un statut HTTP 401 ou 403, vérifiez que la configuration fournit l’authentification Bearer attendue. Si elle indique une variable d’environnement, définissez-la dans l’environnement qui lance le Hub et rouvrez l’application.
3. Si le journal signale un JSON invalide ou une requête HTTP rejetée, relevez la méthode et le statut. Vérifiez que l’URL est celle de l’endpoint HTTP Streamable et qu’il accepte l’initialisation MCP. Après avoir corrigé la configuration, utilisez **Déconnecter**, actualisez l’inventaire et rechargez les outils.

Ouvrez **Journal** dans l’application de bureau pour consulter ses opérations. Pour diagnostiquer une requête envoyée par un processus MCP séparé, appelez `mcp_journal` depuis cette même connexion. Chaque journal couvre son propre processus.

## Un client ne parvient pas à joindre le Hub {#client}

En stdio, vérifiez que la configuration copiée cible l’exécutable `CyMCPHubMcp` actuel et que le client peut le lancer. En HTTP, vérifiez que le Hub tourne, que le **Service MCP (HTTP)** est actif et que le client utilise la configuration copiée après activation. La page [Connexion et outils](doc:mcp#connect) détaille les deux modes.
