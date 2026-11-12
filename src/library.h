#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "textures.h"

struct Game {
    std::filesystem::path root;
    std::string title;
    std::string title_id;
    std::string version;
    std::filesystem::path background_path;
    TexturePtr icon;
};

class Library {
public:
    // Parcourt chaque dossier : il peut être lui-même un jeu, ou contenir des jeux.
    void Scan(SDL_Renderer* renderer, const std::vector<std::filesystem::path>& dirs);

    static bool IsGameFolder(const std::filesystem::path& dir);

    std::vector<Game>& Games() { return games_; }

private:
    void AddGame(SDL_Renderer* renderer, const std::filesystem::path& root);

    std::vector<Game> games_;
};
