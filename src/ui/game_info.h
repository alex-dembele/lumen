#pragma once

#include <cstdint>
#include <future>
#include <vector>

#include <imgui.h>

class App;

// Fiche d'informations d'un jeu (bouton △) : métadonnées, mise à jour installée,
// taille sur le disque et compatibilité signalée par la communauté shadPS4.
class GameInfoPanel {
public:
    void Open(App& app, int game_index);
    // Renvoie false quand l'utilisateur ferme la fiche.
    bool Draw(App& app, ImVec2 size);

private:
    int index_ = -1;
    bool focus_first_ = false;
    std::future<std::uintmax_t> size_;
    std::vector<std::future<std::uintmax_t>> abandoned_;
    std::uintmax_t size_bytes_ = 0;
    bool size_ready_ = false;
};
