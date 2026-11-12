#include "library.h"

#include <algorithm>

#include "sfo.h"

namespace fs = std::filesystem;

namespace {

constexpr int kIconMaxSize = 256;

std::string FormatVersion(const Sfo& sfo) {
    const auto ver = sfo.GetString("APP_VER").value_or(sfo.GetString("VERSION").value_or(""));
    return ver.empty() ? std::string{} : "v" + ver;
}

} // namespace

bool Library::IsGameFolder(const fs::path& dir) {
    std::error_code ec;
    return fs::is_regular_file(dir / "eboot.bin", ec) &&
           fs::is_regular_file(dir / "sce_sys" / "param.sfo", ec);
}

void Library::Scan(SDL_Renderer* renderer, const std::vector<fs::path>& dirs) {
    games_.clear();

    for (const auto& dir : dirs) {
        if (IsGameFolder(dir)) {
            AddGame(renderer, dir);
            continue;
        }
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_directory(ec) && IsGameFolder(entry.path())) {
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
    game.icon = LoadTexture(renderer, root / "sce_sys" / "icon0.png", kIconMaxSize);

    std::error_code ec;
    if (fs::is_regular_file(root / "sce_sys" / "pic1.png", ec)) {
        game.background_path = root / "sce_sys" / "pic1.png";
    } else if (fs::is_regular_file(root / "sce_sys" / "pic0.png", ec)) {
        game.background_path = root / "sce_sys" / "pic0.png";
    }

    games_.push_back(std::move(game));
}
