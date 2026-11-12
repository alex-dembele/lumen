#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <SDL3/SDL.h>
#include <imgui.h>

enum class CompatStatus { Unknown, Nothing, Boots, Menus, Ingame, Playable };

struct CompatEntry {
    CompatStatus status = CompatStatus::Unknown;
    std::string tested_version;
    std::string last_tested;
    std::string issue_url;
};

// Base de compatibilité communautaire de shadPS4 (shadps4-game-compatibility), téléchargée
// avec curl au plus une fois par jour et lue pour le système d'exploitation courant.
class Compatibility {
public:
    ~Compatibility();

    void Init(const std::filesystem::path& config_dir);
    // Renvoie true quand une nouvelle base vient d'être chargée.
    bool Update();

    const CompatEntry* Find(const std::string& title_id) const;
    bool Loaded() const { return !entries_.empty(); }

    static const char* Label(CompatStatus status);
    static const char* Description(CompatStatus status);
    static ImU32 Color(CompatStatus status);

private:
    bool Load();
    void StartDownload();

    std::filesystem::path file_;
    std::filesystem::path temp_file_;
    SDL_Process* download_ = nullptr;
    std::unordered_map<std::string, CompatEntry> entries_;
};
