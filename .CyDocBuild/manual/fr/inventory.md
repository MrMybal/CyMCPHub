---
id: inventory
title: Explorer l’inventaire
description: Trouver les connecteurs locaux, consulter leurs outils et analyser les logiciels.
section: Utiliser CyMCPHub
order: 1
revision: 1
sourceRevision: 1
parent: welcome
---

## Serveurs et connecteurs {#connectors}

**Serveurs MCP** regroupe les connecteurs de même nom. Chaque source, transport et annotation reste associé à son connecteur. La recherche et le filtre d’activité affinent la liste des groupes.

**MCP confirmé** indique qu’un endpoint HTTP local a répondu à l’initialisation MCP. La découverte TCP teste `/mcp` sur les ports en écoute sur la boucle locale. Le Hub peut aussi vérifier un chemin HTTP local configuré. **Processus repéré** signifie que la commande d’un connecteur stdio configuré correspond à un processus local lisible. L’authentification et les droits d’accès aux processus peuvent influer sur ces résultats.

Le Hub lit les réglages courants de Codex, Claude Desktop, Claude Code, VS Code, Cursor, Gemini CLI, Windsurf et OpenCode. Les données de connexion masquent les valeurs des identifiants configurés. Récupérez-les localement depuis leur fichier source.

## Appeler les outils et lire les ressources {#actions}

Dans **Outils**, chargez la liste du connecteur sélectionné et choisissez une action. Consultez son schéma, saisissez ses arguments JSON et exécutez-la. **Ressources** permet de lister et lire les URI. Une connexion stdio lance un processus dédié. **Déconnecter** ferme la session sélectionnée. Le **Journal** indique les opérations, les statuts HTTP et les diagnostics de l’assistant. Copiez-le en JSON Lines pour partager un diagnostic.

## Rechercher des SDK et des CLI {#software}

Dans **SDK** ou **CLI**, choisissez un dossier ou un fichier. Le Hub recherche des références de compilation CyMCPSDK, des noms de bibliothèques, des marqueurs binaires et des noms de CLI connues. Il lit les fichiers sans les exécuter. L’inventaire CLI peut inclure les exécutables trouvés dans le `PATH`. Chaque résultat affiche son chemin et les indices de détection.
