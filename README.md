# Lumen

Une interface de console façon PS4 pour lancer ses jeux PS4 sur PC (Windows, Linux, macOS).
Lumen gère la bibliothèque et l'expérience « canapé ». L'émulation est assurée par le cœur
[shadPS4](https://github.com/shadps4-emu/shadPS4), lancé comme programme séparé.

![Écran d'accueil](docs/accueil.png)

## Objectifs

- **Simple** : on glisse un dossier de jeu dans la fenêtre, l'icône apparaît, ✕ pour jouer.
- **Beau** : écran d'accueil inspiré de la PS4 (tuiles, fond du jeu, vagues lumineuses).
- **Léger** : un exécutable autonome d'environ 5 Mo (C++20, SDL3, Dear ImGui). Il redessine
  l'écran seulement quand quelque chose bouge et se réduit pendant que le jeu tourne. Le fond
  animé peut être désactivé.
- **Manette d'abord** : DualShock, DualSense et Xbox reconnues directement via SDL3. Le clavier
  fonctionne aussi.

## Utilisation

1. Récupérez [shadPS4](https://github.com/shadps4-emu/shadPS4/releases) et placez `shadPS4`
   (ou `shadPS4.exe`) à côté de Lumen. Vous pouvez aussi indiquer son chemin dans les Réglages.
2. Ajoutez vos jeux : glissez un dossier dans la fenêtre, ou utilisez la tuile **+**. Un dossier
   peut être un jeu (`CUSA00001/` contenant `eboot.bin` et `sce_sys/param.sfo`) ou un dossier qui
   en contient plusieurs.
3. Choisissez un jeu et appuyez sur ✕ / Entrée.

| Action | Manette | Clavier |
|---|---|---|
| Naviguer | Croix directionnelle / stick gauche | ← → |
| Démarrer | ✕ | Entrée |
| Réglages | △ / Options | Tab |
| Retour / forcer l'arrêt du jeu | ○ | Échap |
| Plein écran | | F11 |

Options en ligne de commande : `--boot CUSA00001` lance directement un jeu (pratique pour un
raccourci sur le bureau), `--config-dir <dossier>` utilise un autre dossier de réglages,
`--screenshot <fichier.png>` rend l'interface hors écran et enregistre une capture.

> Lumen ne fournit ni jeux, ni firmware, ni clés. Utilisez uniquement des copies de jeux que
> vous avez extraites vous-même de votre console.

## Compiler

Prérequis : CMake ≥ 3.24, un compilateur C++20 et Git. SDL3, Dear ImGui et stb sont téléchargés
automatiquement.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Sous Linux, installez d'abord les en-têtes X11/Wayland requis par SDL3, par exemple sur
Debian/Ubuntu :

```bash
sudo apt install ninja-build libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev libegl1-mesa-dev libdbus-1-dev libudev-dev
```

Pour tester sans vrai jeu, `tools/make_test_games.py <dossier>` génère des faux dossiers de
jeux (avec icônes et fonds d'écran).

## Organisation du code

| Fichier | Rôle |
|---|---|
| `src/app.*` | Boucle principale, entrées clavier/manette, glisser-déposer, notifications |
| `src/ui/home.*` | Écran d'accueil type PS4 (tuiles, fond, animations) |
| `src/ui/settings_panel.*` | Panneau des réglages |
| `src/ui/theme.*` | Couleurs, polices, symboles ✕ ○ △ □ |
| `src/library.*` | Recherche des jeux et lecture des métadonnées |
| `src/sfo.*` | Lecteur du format `param.sfo` |
| `src/emulator.*` | Lancement et suivi du processus shadPS4 |
| `src/settings.*` | Réglages enregistrés dans le dossier utilisateur |

## Feuille de route

- [x] Écran d'accueil, bibliothèque, lancement de shadPS4, réglages
- [ ] Sons de navigation et musique d'accueil (`snd0.at9` des jeux)
- [ ] Prise en charge des archives `.zar` de shadPS4
- [ ] Écran d'informations du jeu (mises à jour, DLC, sauvegardes)
- [ ] Réglages par jeu (résolution, patchs), transmis à shadPS4
- [ ] Retour au menu par le bouton PS pendant le jeu (nécessite une modification du cœur)
- [ ] Paquets d'installation Windows / AppImage / .dmg
