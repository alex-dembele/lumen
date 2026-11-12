#pragma once

#include <filesystem>
#include <string>

// Réglages shadPS4 recommandés pour certains jeux, appliqués via le fichier de configuration
// par jeu de shadPS4 (user/custom_configs/<TITLE_ID>.json).
struct GameProfile {
    const char* name;
    const char* summary; // affiché dans la fiche du jeu
};

const GameProfile* FindProfile(const std::string& title_id);

// Écrit le profil du jeu s'il en a un. Un fichier déjà présent n'est remplacé que s'il a été
// écrit par Lumen : une configuration créée à la main n'est jamais écrasée.
bool ApplyProfile(const std::filesystem::path& emulator, const std::string& title_id,
                  std::string& error);
