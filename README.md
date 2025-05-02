# Système de Coordination de Bras Robotiques

Ce projet simule un système distribué où plusieurs bras robotiques doivent acquérir des outils partagés pour effectuer des tâches d’assemblage. Un serveur central (le gestionnaire d'outils) gère la synchronisation et l’accès concurrent aux ressources critiques (outils), en utilisant des stratégies d’évitement de blocage.
Ce projet est une adaptation réaliste du problème des philosophes dans un contexte industriel.

## 📦 Fichiers Principaux

- `client/bras_robotique.c` : programme client simulant un bras robotisé.
- `serveur/gestionnaire_outils.c` : serveur de gestion des outils et stratégie d’accès.
- `include/commun.h` : définitions communes (constantes, structures, macros).
- `Makefile` : permet de compiler facilement les programmes serveur et clients.

## ⚙️ Compilation

Lancer :

```bash
make
```

Cela génère deux exécutables :
- `bras_robotique` : le client représentant un bras robotisé.
- `gestionnaire_outils` : le serveur de gestion des outils.

Vous pouvez nettoyer les fichiers générés avec :

```bash
make clean
```

## 🚀 Exécution

### 1. Démarrer le serveur

```bash
./gestionnaire_outils
```

### 2. Lancer un ou plusieurs bras robotiques

```bash
./bras_robotique <bras_id> [outil1] [outil2]
```

- `<bras_id>` : identifiant unique du bras.
- `[outil1] [outil2]` : (optionnel) identifiants des outils à utiliser (par défaut : 1 et 2).

Exemple :

```bash
./bras_robotique 1 0 1
./bras_robotique 2 2 3
```

## 🧠 Stratégies de Gestion de Conflit

Le serveur supporte plusieurs stratégies pour prévenir les interblocages (deadlocks) :

- `wait-die` : les bras plus jeunes meurent lorsqu’un conflit survient.
- `wound-wait` : les bras plus anciens forcent les plus jeunes à relâcher les outils.

Vous pouvez changer dynamiquement la stratégie avec la commande :

```
set_strategy wait-die
set_strategy wound-wait
```

## 📡 Communication

La communication se fait par socket TCP sur le port 12345 (modifiable dans `include/commun.h`).

Le protocole entre bras et serveur repose sur des messages texte :
- `id <bras_id>`
- `demande_2_outils <id1> <id2>`
- `liberation_outil <id>`
- `sync`
- `set_strategy <stratégie>`

## 📝 Journalisation

Le serveur enregistre toutes les actions (acquisition, libération, changement de stratégie, etc.) avec un timestamp dans un journal mémoire.