# Étapes pour créer un logiciel de partage d'écran en UDP

Ce document détaille les étapes pour développer un logiciel de partage d'écran P2P qui peut être minimisé dans la zone de notification de Windows (système tray). L'application utilise raylib pour l'interface graphique et rnet.h pour la communication réseau en UDP.

## État actuel du projet

Le projet implémente actuellement une capture d'écran avancée avec support des API Windows (GDI/BitBlt) pour capturer l'écran global, le support multi-écrans, et la détection de changements. L'adresse IP du client est affichée dans une barre supérieure. Les prochaines étapes consistent à implémenter le partage P2P de l'écran et l'intégration avec la zone de notification Windows.

## Outils et bibliothèques utilisés

- **raylib**: Une bibliothèque C simple pour développer des jeux/applications qui fournit des fonctions pour créer des fenêtres, gérer l'input, et afficher des graphiques.
- **rnet.h**: Une fine couche d'abstraction pour la communication réseau UDP (incluse dans notre projet).
- **nob.c/nob.h**: Un système de build simple pour C qui nous permet de compiler notre projet.
- **Windows GDI**: API Windows utilisée pour la capture d'écran globale (BitBlt).

## Structure du projet

```
nob.c                  # Système de build
nob.h                  # Header du système de build
README.md              # Ce document
include/               # Fichiers d'en-tête
  ├── capture.h        # Définitions pour la capture d'écran
  ├── network.h        # Définitions pour la communication réseau
  ├── raylib.h         # API de raylib
  ├── raymath.h        # Fonctions mathématiques de raylib
  ├── rlgl.h           # Fonctions OpenGL de raylib
  ├── rnet.h           # API de communication réseau
  └── ui.h             # Définitions pour l'interface utilisateur
lib/                   # Bibliothèques
  ├── libraylib.a      # Bibliothèque statique raylib
  ├── libraylibdll.a   # Bibliothèque d'importation raylib
  └── raylib.dll       # Bibliothèque dynamique raylib
src/                   # Code source
  ├── capture.c        # Implémentation de la capture d'écran
  └── main.c           # Point d'entrée de l'application
```

## Étapes complétées

### Étape 1: Configuration du projet
- ✅ Mise en place de la structure du projet
- ✅ Intégration de raylib pour l'interface graphique
- ✅ Configuration du système de build avec nob.c

### Étape 2: Implémentation de la capture d'écran basique
- ✅ Création d'une fenêtre avec raylib
- ✅ Implémentation de la capture d'écran avec `LoadImageFromScreen()`
- ✅ Conversion de l'image en texture pour l'affichage
- ✅ Implémentation d'une compression de base
- ✅ Affichage de l'adresse IP du client dans la barre supérieure

### Étape 3: Amélioration de la capture d'écran
- ✅ Implémentation de la capture avec les API Windows GDI (BitBlt)
- ✅ Support de la capture d'écran global au lieu de la fenêtre raylib
- ✅ Détection et support des configurations multi-écrans
- ✅ Optimisation de la compression d'image
- ✅ Implémentation de la détection de changements entre captures
- ✅ Ajustement dynamique de la qualité selon les changements détectés

## Prochaines étapes

### Étape 4: Implémentation du partage P2P
1. Mettre en place le système de communication P2P avec rnet.h:
   - Initialiser la bibliothèque réseau
   - Développer le système de connexion par adresse IP
   - Implémenter la transmission des captures d'écran

2. Ajouter la gestion granulaire des connexions:
   - Interface pour entrer/sauvegarder des adresses IP de pairs
   - Option pour autoriser/bloquer le partage avec des pairs spécifiques
   - Panneau de contrôle des pairs connectés avec statut en temps réel
   - Possibilité de diffuser à tous les pairs ou seulement à une sélection

3. Implémenter le chiffrement des données pour la sécurité:
   - Chiffrement des flux vidéo
   - Authentication entre les pairs
   - Protection par mot de passe des sessions de partage

### Étape 5: Interface utilisateur avancée
1. Créer une interface utilisateur complète:
   - Onglets pour naviguer entre les différentes fonctionnalités
   - Liste des pairs connectés avec statuts et options
   - Paramètres de qualité et performances
   - Indicateurs visuels de l'état du partage et de la qualité du réseau

2. Améliorer l'expérience utilisateur:
   - Ajout de notifications pour les événements importants
   - Interface intuitive et responsive
   - Thèmes visuels (clair/sombre)

### Étape 6: Support de la zone de notification (system tray)
1. Implémenter l'intégration avec la zone de notification Windows:
   - Icône dans la zone de notification
   - Menu contextuel pour les fonctions principales
   - Poursuite du partage en arrière-plan
   - Notifications système pour les connexions/déconnexions

### Étape 7: Optimisation et fiabilité
1. Optimiser les performances:
   - Utilisation de threads pour les opérations intensives
   - Gestion intelligente de la bande passante
   - Adaptation dynamique de la qualité selon les conditions réseau

2. Améliorer la fiabilité:
   - Système robuste de reconnexion automatique
   - Gestion des erreurs et exceptions
   - Journalisation avancée pour le débogage

### Étape 8: Tests et déploiement
1. Tests approfondis:
   - Tests sur différentes configurations matérielles
   - Tests dans diverses conditions réseau
   - Tests de sécurité

2. Déploiement:
   - Création d'un installeur
   - Documentation utilisateur
   - Support pour les mises à jour

## Remarques importantes

- Le logiciel est conçu comme une solution P2P sans serveur central, permettant un partage direct entre utilisateurs.
- Le protocole UDP est utilisé pour sa rapidité, avec des mécanismes de fiabilité intégrés pour les données importantes.
- Le chiffrement des données est une priorité pour garantir la confidentialité des écrans partagés.
- L'application est optimisée pour Windows mais peut être adaptée à d'autres plateformes en modifiant les fonctionnalités spécifiques à Windows.