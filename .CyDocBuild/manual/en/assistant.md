---
id: assistant
title: Assistant and Skills
description: Open the separate chat window and collect or author reusable Skills.
section: Using CyMCPHub
order: 2
revision: 1
parent: welcome
---

## Work with the current tab {#chat}

**Assistant** opens a second window. Select and connect an available provider such as the Codex CLI. The chat receives the Hub's active tab and selection. Use **Open in assistant** from a connector to give the chat access to that connector's tools. Ask the assistant to inspect an action before running it.

```image
{"id":"chat-window","src":"assets/images/hub-assistant.png","alt":"Separate CyMCPHub assistant window showing the active MCP servers tab.","title":"Assistant window with the current Hub tab","controls":"minimal"}
```

## Collect or write a Skill {#skills}

In a software scan, check the discovered `SKILL.md` files. From a connector's **Skills** tab, collect Markdown exposed as a resource or through `search_skills` and `get_skill`. The **Skills** library lets you open and copy collected documents.

Use **Create with assistant** to prepare a request for a new Skill for the selected connector. Ask the assistant to review the tool descriptions and save a Markdown document with YAML `name` and `description` fields. Review the saved text before using it in another client.
