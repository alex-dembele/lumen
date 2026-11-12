#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace {

std::string DefaultUserName() {
    for (const char* var : {"USERNAME", "USER"}) {
        if (const char* value = std::getenv(var); value && *value) {
            return value;
        }
    }
    return "Joueur 1";
}

std::filesystem::path PathFromUtf8(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

std::string PathToUtf8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

} // namespace

// Format : une ligne "clé=valeur" par réglage, "game_dir" peut être répété.
void Settings::Load(const std::filesystem::path& file) {
    user_name = DefaultUserName();

    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "emulator") {
            emulator_path = PathFromUtf8(value);
        } else if (key == "game_dir") {
            AddGameDir(PathFromUtf8(value));
        } else if (key == "user") {
            user_name = value;
        } else if (key == "fullscreen") {
            fullscreen = value == "1";
        } else if (key == "animations") {
            animations = value == "1";
        } else if (key == "minimize_while_playing") {
            minimize_while_playing = value == "1";
        }
    }
}

bool Settings::Save(const std::filesystem::path& file) const {
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        return false;
    }
    out << "emulator=" << PathToUtf8(emulator_path) << '\n';
    for (const auto& dir : game_dirs) {
        out << "game_dir=" << PathToUtf8(dir) << '\n';
    }
    out << "user=" << user_name << '\n';
    out << "fullscreen=" << (fullscreen ? 1 : 0) << '\n';
    out << "animations=" << (animations ? 1 : 0) << '\n';
    out << "minimize_while_playing=" << (minimize_while_playing ? 1 : 0) << '\n';
    return static_cast<bool>(out);
}

bool Settings::AddGameDir(const std::filesystem::path& dir) {
    if (dir.empty() || std::find(game_dirs.begin(), game_dirs.end(), dir) != game_dirs.end()) {
        return false;
    }
    game_dirs.push_back(dir);
    return true;
}
