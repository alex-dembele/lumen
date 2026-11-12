#include "compat.h"

#include <chrono>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

constexpr const char* kUrl =
    "https://github.com/shadps4-compatibility/shadps4-game-compatibility/releases/latest/"
    "download/compatibility_data.json";
constexpr const char* kIssueUrl =
    "https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/";
constexpr auto kMaxAge = std::chrono::hours(24);

#if defined(_WIN32)
constexpr const char* kOsKey = "os-windows";
#elif defined(__APPLE__)
constexpr const char* kOsKey = "os-macOS";
#else
constexpr const char* kOsKey = "os-linux";
#endif

CompatStatus ParseStatus(const std::string& label) {
    if (label == "status-playable") return CompatStatus::Playable;
    if (label == "status-ingame") return CompatStatus::Ingame;
    if (label == "status-menus") return CompatStatus::Menus;
    if (label == "status-boots") return CompatStatus::Boots;
    if (label == "status-nothing") return CompatStatus::Nothing;
    return CompatStatus::Unknown;
}

// Lit un champ texte ou numérique sans lever d'exception si le format évolue.
std::string GetString(const nlohmann::json& obj, const char* key) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        return {};
    }
    if (it->is_string()) {
        return it->get<std::string>();
    }
    if (it->is_number_integer()) {
        return std::to_string(it->get<long long>());
    }
    return {};
}

std::string ToUtf8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

} // namespace

Compatibility::~Compatibility() {
    if (download_) {
        SDL_DestroyProcess(download_);
    }
}

void Compatibility::Init(const fs::path& config_dir) {
    file_ = config_dir / "compatibility_data.json";
    temp_file_ = config_dir / "compatibility_data.json.part";
    const bool loaded = Load();

    std::error_code ec;
    const auto modified = fs::last_write_time(file_, ec);
    const bool stale = ec || fs::file_time_type::clock::now() - modified > kMaxAge;
    if (!loaded || stale) {
        StartDownload();
    }
}

void Compatibility::StartDownload() {
    const std::string out = ToUtf8(temp_file_);
    const char* args[] = {"curl", "-sfL", "--max-time", "60", "-o", out.c_str(), kUrl, nullptr};
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args));
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
    download_ = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
}

bool Compatibility::Update() {
    int code = 0;
    if (!download_ || !SDL_WaitProcess(download_, false, &code)) {
        return false;
    }
    SDL_DestroyProcess(download_);
    download_ = nullptr;

    std::error_code ec;
    if (code != 0) {
        fs::remove(temp_file_, ec);
        return false;
    }
    fs::rename(temp_file_, file_, ec);
    return !ec && Load();
}

bool Compatibility::Load() {
    std::ifstream in(file_);
    if (!in) {
        return false;
    }
    const auto doc = nlohmann::json::parse(in, nullptr, false);
    if (!doc.is_object()) {
        return false;
    }

    std::unordered_map<std::string, CompatEntry> entries;
    for (const auto& [serial, per_os] : doc.items()) {
        if (!per_os.is_object() || !per_os.contains(kOsKey)) {
            continue;
        }
        const auto& e = per_os[kOsKey];
        if (!e.is_object()) {
            continue;
        }
        CompatEntry entry;
        entry.status = ParseStatus(GetString(e, "status"));
        entry.tested_version = GetString(e, "version");
        entry.last_tested = GetString(e, "last_tested").substr(0, 10);
        if (const auto issue = GetString(e, "issue_number"); !issue.empty()) {
            entry.issue_url = kIssueUrl + issue;
        }
        entries.emplace(serial, std::move(entry));
    }
    entries_ = std::move(entries);
    return !entries_.empty();
}

const CompatEntry* Compatibility::Find(const std::string& title_id) const {
    const auto it = entries_.find(title_id);
    return it == entries_.end() ? nullptr : &it->second;
}

const char* Compatibility::Label(CompatStatus status) {
    switch (status) {
    case CompatStatus::Playable: return "Jouable";
    case CompatStatus::Ingame: return "En jeu";
    case CompatStatus::Menus: return "Menus";
    case CompatStatus::Boots: return "Démarre";
    case CompatStatus::Nothing: return "Ne démarre pas";
    case CompatStatus::Unknown: break;
    }
    return "Non testé";
}

const char* Compatibility::Description(CompatStatus status) {
    switch (status) {
    case CompatStatus::Playable: return "Jouable du début à la fin, avec au plus quelques défauts mineurs.";
    case CompatStatus::Ingame: return "Le jeu se lance mais des bugs graphiques, sonores ou de performance gênent.";
    case CompatStatus::Menus: return "Le jeu atteint les menus mais plante ou bloque avant de pouvoir jouer.";
    case CompatStatus::Boots: return "Le jeu démarre mais n'atteint pas les menus.";
    case CompatStatus::Nothing: return "Le jeu ne démarre pas.";
    case CompatStatus::Unknown: break;
    }
    return "Aucun rapport de la communauté pour ce jeu sur ce système.";
}

ImU32 Compatibility::Color(CompatStatus status) {
    switch (status) {
    case CompatStatus::Playable: return IM_COL32(70, 200, 110, 255);
    case CompatStatus::Ingame: return IM_COL32(240, 200, 60, 255);
    case CompatStatus::Menus: return IM_COL32(245, 140, 50, 255);
    case CompatStatus::Boots: return IM_COL32(230, 90, 60, 255);
    case CompatStatus::Nothing: return IM_COL32(200, 50, 60, 255);
    case CompatStatus::Unknown: break;
    }
    return IM_COL32(150, 160, 175, 255);
}
