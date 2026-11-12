#include "emulator.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
constexpr const char* kExeName = "shadPS4.exe";
constexpr char kPathSep = ';';
#else
constexpr const char* kExeName = "shadPS4";
constexpr char kPathSep = ':';
#endif

std::string ToUtf8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

} // namespace

Emulator::~Emulator() {
    if (process_) {
        SDL_DestroyProcess(process_);
    }
}

fs::path Emulator::FindDefault() {
    std::error_code ec;
    if (const char* base = SDL_GetBasePath()) {
        for (const fs::path& candidate :
             {fs::path(base) / kExeName, fs::path(base) / "shadPS4" / kExeName}) {
            if (fs::is_regular_file(candidate, ec)) {
                return candidate;
            }
        }
    }
    if (const char* path_env = std::getenv("PATH")) {
        std::stringstream ss(path_env);
        std::string dir;
        while (std::getline(ss, dir, kPathSep)) {
            if (!dir.empty() && fs::is_regular_file(fs::path(dir) / kExeName, ec)) {
                return fs::path(dir) / kExeName;
            }
        }
    }
    return {};
}

bool Emulator::Launch(const fs::path& emulator, const fs::path& game, bool fullscreen,
                      std::string& error) {
    if (process_) {
        error = "Un jeu est déjà en cours.";
        return false;
    }
    std::error_code ec;
    if (emulator.empty() || !fs::is_regular_file(emulator, ec)) {
        error = "shadPS4 est introuvable. Indiquez son emplacement dans les Réglages.";
        return false;
    }

    const std::string exe = ToUtf8(emulator);
    const std::string game_arg = ToUtf8(game);
    const std::vector<const char*> args = {exe.c_str(),
                                           "--fullscreen",
                                           fullscreen ? "true" : "false",
                                           "-g",
                                           game_arg.c_str(),
                                           nullptr};

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER,
                           const_cast<char**>(args.data()));
    const std::string work_dir = ToUtf8(emulator.parent_path());
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING,
                          work_dir.c_str());
    process_ = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);

    if (!process_) {
        error = std::string("Impossible de lancer shadPS4 : ") + SDL_GetError();
        return false;
    }
    return true;
}

std::optional<int> Emulator::PollExit() {
    if (!process_) {
        return std::nullopt;
    }
    int code = 0;
    if (!SDL_WaitProcess(process_, false, &code)) {
        return std::nullopt;
    }
    SDL_DestroyProcess(process_);
    process_ = nullptr;
    return code;
}

void Emulator::Stop() {
    if (process_) {
        SDL_KillProcess(process_, false);
    }
}
