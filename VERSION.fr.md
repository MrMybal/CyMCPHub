# CyMCPHub 0.3.2

- Interface revue avec une barre latérale CyUI, une page Réglages et une barre d’état.
- Actualisation de l’inventaire en arrière-plan, port et mode d’accès HTTP configurables, et réglages de démarrage distincts pour l’application et son service MCP.
- Journal avec recherche et filtre des problèmes, dont les refus d’authentification pendant la découverte des MCP.
- Aperçu Markdown des Skills collectés et filtre des connecteurs à vérifier dans l’inventaire.

- Recherche des releases GitHub stables depuis l’application.
- Téléchargement, vérification et lancement de l’installateur de mise à jour Windows x64.
- Installation Windows par utilisateur avec raccourci du menu Démarrer et désinstallation.

- Inventaire des serveurs MCP HTTP locaux et stdio configurés, regroupés par nom de connecteur.
- Analyse des SDK et CLI, annotations, journal et bibliothèque partagée de fichiers `SKILL.md`.
- Chat IA dans une fenêtre séparée avec l’onglet actif et l’élément sélectionné en contexte.
- Service MCP local avec catalogue consultable et export en Markdown.
- Versions Windows x64 et Ubuntu 24.04 x64. Le workflow de paquetage cible aussi macOS 14+ sur Intel et Apple Silicon. Cette cible reste à compiler et à valider sur un bureau macOS.

Sous Linux, la zone de notification dépend de la prise en charge d’AppIndicator par le bureau. L’inventaire voit les processus et ports accessibles au compte courant. L’interface des connecteurs couvre le transport stdio et HTTP Streamable pour les outils et ressources.
