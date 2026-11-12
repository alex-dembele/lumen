#include "profiles.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

constexpr const char* kMarker = "_lumen_profile";

// Moteur Naughty Dog (Uncharted 4, The Lost Legacy) :
// - sous Windows, la « red zone » sous la pile du jeu peut être écrasée par Windows : sans la
//   protection de shadPS4, CUSA00918 plante au démarrage dans ses fibers (rapport #3200, v0.21),
//   avec elle CUSA04529 atteint les menus (rapport #2634, v0.20) ;
// - ses calculs GPU (raycasts, etc.) doivent revenir en mémoire du jeu : sans readbacks, des
//   asserts internes se déclenchent dès le début de la partie (shadPS4 PR #5546).
constexpr GameProfile kNaughtyDog{
    "Moteur Naughty Dog",
    "Protection red zone (Windows) et readbacks GPU activés",
};

// Identifiants relevés dans la base de compatibilité shadPS4.
constexpr const char* kNaughtyDogSerials[] = {
    "CUSA00341", "CUSA00917", "CUSA00918", "CUSA04529", // Uncharted 4
    "CUSA07737", "CUSA07875",                           // The Lost Legacy
};

nlohmann::json Overrides(const GameProfile& profile) {
    nlohmann::json j;
    j[kMarker] = profile.name;
#ifdef _WIN32
    j["General"]["redzone_patches"] = true;
#endif
    j["GPU"]["readbacks_mode"] = 1; // Relaxed : compromis entre exactitude et performances
    return j;
}

// Même logique que shadPS4 : dossier « user » à côté de l'exécutable (Lumen le lance depuis son
// dossier), sinon le dossier de données de la plateforme.
fs::path ShadUserDir(const fs::path& emulator) {
    std::error_code ec;
    const fs::path portable = emulator.parent_path() / "user";
    if (fs::is_directory(portable, ec)) {
        return portable;
    }
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        return fs::path(appdata) / "shadPS4";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return fs::path(home) / "Library" / "Application Support" / "shadPS4";
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
        return fs::path(xdg) / "shadPS4";
    }
    if (const char* home = std::getenv("HOME")) {
        return fs::path(home) / ".local" / "share" / "shadPS4";
    }
#endif
    return portable;
}

} // namespace

const GameProfile* FindProfile(const std::string& title_id) {
    const auto end = std::end(kNaughtyDogSerials);
    if (std::find(std::begin(kNaughtyDogSerials), end, title_id) != end) {
        return &kNaughtyDog;
    }
    return nullptr;
}

bool ApplyProfile(const fs::path& emulator, const std::string& title_id, std::string& error) {
    const GameProfile* profile = FindProfile(title_id);
    if (!profile) {
        return false;
    }

    const fs::path dir = ShadUserDir(emulator) / "custom_configs";
    const fs::path file = dir / (title_id + ".json");
    std::error_code ec;
    if (fs::exists(file, ec)) {
        std::ifstream in(file);
        const auto existing = nlohmann::json::parse(in, nullptr, false);
        if (!existing.is_object() || !existing.contains(kMarker)) {
            return false;
        }
    }

    fs::create_directories(dir, ec);
    std::ofstream out(file, std::ios::trunc);
    if (!out) {
        error = "Impossible d'écrire les réglages recommandés pour " + title_id;
        return false;
    }
    out << Overrides(*profile).dump(4) << '\n';
    return static_cast<bool>(out);
}
