#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Settings {
    std::filesystem::path emulator_path;
    std::vector<std::filesystem::path> game_dirs;
    std::string user_name;
    bool fullscreen = true;
    bool animations = true;
    bool minimize_while_playing = true;
    bool sounds = true;
    bool music = true;

    void Load(const std::filesystem::path& file);
    bool Save(const std::filesystem::path& file) const;

    bool AddGameDir(const std::filesystem::path& dir);
};
