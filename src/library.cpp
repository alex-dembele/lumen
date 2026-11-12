#include "library.h"

#include <algorithm>
#include <cstdio>

#include "sfo.h"

namespace fs = std::filesystem;

namespace {

constexpr int kIconMaxSize = 256;
constexpr const char* kOverlaySuffixes[] = {"-UPDATE", "-patch", "-mods"};

std::string FormatVersion(const Sfo& sfo) {
    const auto ver = sfo.GetString("APP_VER").value_or(sfo.GetString("VERSION").value_or(""));
    return ver.empty() ? std::string{} : "v" + ver;
}

std::string FormatFirmware(std::uint32_t system_ver) {
    if (system_ver == 0) {
        return {};
    }
    // 0x05050000 -> « 5.05 »
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%x.%02x", system_ver >> 24, (system_ver >> 16) & 0xFF);
    return buf;
}

std::string CategoryName(const std::string& category) {
    if (category == "gd") return "Jeu";
    if (category == "gp") return "Mise à jour";
    if (category == "ac") return "Contenu additionnel";
    return category;
}

fs::path WithSuffix(const fs::path& root, const char* suffix) {
    return root.parent_path() / (root.filename().string() + suffix);
}

} // namespace

bool Library::IsOverlayFolder(const fs::path& dir) {
    const std::string name = dir.filename().string();
    for (const char* suffix : kOverlaySuffixes) {
        const std::string s(suffix);
        if (name.size() > s.size() && name.compare(name.size() - s.size(), s.size(), s) == 0) {
            return true;
        }
    }
    return false;
}

bool Library::IsGameFolder(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "eboot.bin", ec) &&
           fs::is_regular_file(dir / "sce_sys" / "param.sfo", ec);
}

void Library::Scan(SDL_Renderer* renderer, const std::vector<fs::path>& dirs) {
    games_.clear();

    for (const auto& dir : dirs) {
        if (IsGameFolder(dir)) {
            if (!IsOverlayFolder(dir)) {
                AddGame(renderer, dir);
            }
            continue;
        }
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_directory(ec) && !IsOverlayFolder(entry.path()) &&
                IsGameFolder(entry.path())) {
                AddGame(renderer, entry.path());
            }
        }
    }

    std::sort(games_.begin(), games_.end(),
              [](const Game& a, const Game& b) { return a.title < b.title; });
}

void Library::AddGame(SDL_Renderer* renderer, const fs::path& root) {
    const bool already_listed = std::any_of(games_.begin(), games_.end(),
                                            [&](const Game& g) { return g.root == root; });
    if (already_listed) {
        return;
    }

    Sfo sfo;
    sfo.Load(root / "sce_sys" / "param.sfo");

    Game game;
    game.root = root;
    game.title = sfo.GetString("TITLE").value_or(root.filename().string());
    game.title_id = sfo.GetString("TITLE_ID").value_or("");
    game.version = FormatVersion(sfo);
    game.category = CategoryName(sfo.GetString("CATEGORY").value_or(""));
    game.required_firmware = FormatFirmware(sfo.GetInt("SYSTEM_VER").value_or(0));

    std::error_code ec;
    for (const char* suffix : {"-UPDATE", "-patch"}) {
        Sfo update;
        if (update.Load(WithSuffix(root, suffix) / "sce_sys" / "param.sfo")) {
            game.update_version = FormatVersion(update);
            break;
        }
    }
    game.has_mods = fs::is_directory(WithSuffix(root, "-mods"), ec);

    // Les images et la musique de la mise à jour remplacent celles du jeu de base.
    const auto find_asset = [&](const char* name) -> fs::path {
        for (const fs::path& base : {WithSuffix(root, "-UPDATE"), WithSuffix(root, "-patch"), root}) {
            if (fs::is_regular_file(base / "sce_sys" / name, ec)) {
                return base / "sce_sys" / name;
            }
        }
        return {};
    };
    game.icon = LoadTexture(renderer, find_asset("icon0.png"), kIconMaxSize);
    game.music_path = find_asset("snd0.at9");

    game.background_path = find_asset("pic1.png");
    if (game.background_path.empty()) {
        game.background_path = find_asset("pic0.png");
    }

    games_.push_back(std::move(game));
}
