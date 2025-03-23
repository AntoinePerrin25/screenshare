# Étapes pour créer un logiciel de partage d'écran en UDP

Ce document détaille les étapes pour développer un logiciel de partage d'écran qui peut être minimisé dans la zone de notification de Windows (système tray). L'application utilise raylib pour l'interface graphique et rnet.h pour la communication réseau en UDP.

## Outils et bibliothèques utilisés

- **raylib**: Une bibliothèque C simple pour développer des jeux/applications. Elle fournit des fonctions pour créer des fenêtres, gérer l'input, et afficher des graphiques.
- **rnet.h**: Une fine couche d'abstraction au-dessus de ENet pour la communication réseau (incluse dans notre projet).
- **nob.c/nob.h**: Un système de build simple pour C qui nous permettra de compiler notre projet.


## Étape 2: Implémentation de la capture d'écran avec raylib

1. Utiliser `InitWindow()` de raylib pour créer une fenêtre (qui pourra être cachée)
2. Implementer la capture d'écran:
   - Utiliser `GetScreenWidth()` et `GetScreenHeight()` pour obtenir la résolution
   - Utiliser `LoadImage()` ou `TakeScreenshot()` pour capturer l'écran
   - Convertir l'image en texture avec `LoadTextureFromImage()`
   - Compresser les données d'image pour la transmission réseau

## Étape 3: Mise en place du système de communication réseau avec rnet.h

1. Initialiser la bibliothèque réseau avec `rnetInit()`
2. Créer un serveur (émetteur) avec `rnetHost()` qui:
   - Diffuse les captures d'écran aux clients connectés avec `rnetBroadcast()`
   - Vérifie les nouvelles connexions avec `rnetReceive()`
3. Créer un client (récepteur) avec `rnetConnect()` qui:
   - Reçoit les données d'image avec `rnetReceive()`
   - Décompresse et affiche l'image reçue

## Étape 4: Implémentation de l'interface utilisateur

1. Créer une interface utilisateur simple avec:
   - Boutons pour démarrer/arrêter le partage d'écran
   - Affichage de l'état de la connexion et des clients connectés
   - Affichage de l'adresse IP de l'utilsateur
   - Champ de saisie pour l'adresse IP des clients pairs
   - Options de configuration (qualité de l'image, taux de rafraîchissement)

2. Gérer les entrées utilisateur:
   - Utiliser `IsKeyPressed()` et `IsMouseButtonPressed()` pour détecter les interactions
   - Implémenter des composants d'UI personnalisés ou utiliser raygui (extension de raylib)

## Étape 5: Support de la zone de notification (system tray)

1. Implémenter l'intégration avec la zone de notification Windows:
   - Utiliser les API Windows pour créer une icône dans la zone de notification
   - Implémenter un menu contextuel pour contrôler l'application depuis la zone de notification
   - Permettre à l'application de continuer à fonctionner en arrière-plan

2. Gérer les événements de minimisation/restauration:
   - Capter l'événement de minimisation pour cacher la fenêtre au lieu de la minimiser
   - Ajouter une option pour restaurer l'application depuis la zone de notification

## Étape 6: Optimisation des performances

1. Optimiser la capture et la transmission d'écran:
   - Détecter les changements entre les images pour n'envoyer que les zones modifiées
   - Implémenter plusieurs niveaux de compression selon la qualité souhaitée
   - Utiliser des threads séparés pour la capture, la compression et l'envoi

2. Optimiser l'utilisation de la bande passante:
   - Adapter dynamiquement la qualité d'image selon la latence réseau
   - Utiliser `RNET_UNRELIABLE` pour les trames moins importantes, `RNET_RELIABLE` pour les trames clés
   - Mettre en place un mécanisme de throttling pour éviter la congestion réseau

## Étape 7: Gestion des erreurs et amélioration de l'expérience utilisateur

1. Implémenter une gestion robuste des erreurs:
   - Gérer les déconnexions et tentatives de reconnexion automatiques
   - Afficher des messages d'erreur clairs à l'utilisateur
   - Journaliser les événements importants pour le débogage

2. Améliorer l'expérience utilisateur:
   - Ajouter des indicateurs de performance (FPS, latence réseau)
   - Permettre de sélectionner un écran spécifique sur les configurations multi-écrans
   - Implémenter des raccourcis clavier globaux pour les actions courantes

## Étape 8: Tests et déploiement

1. Tester l'application dans différentes conditions réseau:
   - Réseau local (LAN)
   - Via Internet avec différentes vitesses de connexion
   - À travers des pare-feu et des NAT

2. Préparer le déploiement:
   - Créer un installeur ou un package portable
   - Documenter l'utilisation de l'application
   - Vérifier la compatibilité avec différentes versions de Windows

## Structure du code source

```
src/
  ├── main.c           # Point d'entrée de l'application
  ├── capture.c        # Fonctions de capture d'écran
  ├── network.c        # Gestion du réseau avec rnet.h
  ├── ui.c             # Interface utilisateur
  ├── tray.c           # Intégration avec la zone de notification
  └── common.h         # Définitions communes

build.c                # Script de build avec nob
```

## Remarques importantes

- L'utilisation de raylib simplifie considérablement le développement multiplateforme, mais notre focus initial est Windows pour la fonctionnalité de zone de notification.
- Le protocole UDP via rnet.h est idéal pour le streaming d'écran car il privilégie la vitesse à la fiabilité.
- Pour une qualité optimale, nous devrons expérimenter avec différentes techniques de compression et de détection de changements d'écran.