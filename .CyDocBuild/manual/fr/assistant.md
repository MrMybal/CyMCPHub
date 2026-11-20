---
id: assistant
title: Assistant et Skills
description: Ouvrir le chat séparé et collecter ou rédiger des Skills réutilisables.
section: Utiliser CyMCPHub
order: 2
revision: 1
sourceRevision: 1
parent: welcome
---

## Travailler avec l’onglet courant {#chat}

**Assistant** ouvre une deuxième fenêtre. Sélectionnez et connectez un fournisseur disponible, par exemple la CLI Codex. Le chat reçoit l’onglet actif et la sélection du Hub. Utilisez **Ouvrir dans l’assistant** depuis la fiche d’un connecteur pour donner au chat accès à ses outils. Demandez à l’assistant d’inspecter une action avant de l’exécuter.

```image
{"id":"chat-window","src":"assets/images/hub-assistant.png","alt":"Fenêtre séparée de l’assistant CyMCPHub qui affiche l’onglet Serveurs MCP en contexte.","title":"Fenêtre de l’assistant et onglet courant du Hub","controls":"minimal"}
```

## Collecter ou rédiger un Skill {#skills}

Après une analyse de logiciel, consultez les fichiers `SKILL.md` détectés. Depuis le sous-onglet **Skills** d’un connecteur, collectez le Markdown fourni comme ressource ou via `search_skills` et `get_skill`. La bibliothèque **Skills** permet d’ouvrir et de copier les documents collectés.

Utilisez **Créer avec l’assistant** pour préparer une demande de Skill pour le connecteur sélectionné. Demandez à l’assistant de consulter les descriptions des outils et d’enregistrer un Markdown avec les champs YAML `name` et `description`. Relisez le texte enregistré avant de l’utiliser dans un autre client.
