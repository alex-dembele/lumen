#include "ui/game_info.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>

#include "app.h"
#include "profiles.h"

namespace fs = std::filesystem;

namespace {

std::string ToUtf8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::uintmax_t FolderSize(const fs::path& root) {
    std::uintmax_t total = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, ec); !ec && it != fs::end(it);
         it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            total += it->file_size(ec);
        }
    }
    return total;
}

std::string FormatSize(std::uintmax_t bytes) {
    if (bytes < 1024 * 1024) {
        return "< 1 Mo";
    }
    char buf[32];
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) {
        std::snprintf(buf, sizeof(buf), "%.1f Go", gb);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f Mo", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return buf;
}

void Row(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
}

} // namespace

void GameInfoPanel::Open(App& app, int game_index) {
    // Un calcul de taille encore en cours est mis de côté : détruire son future bloquerait.
    std::erase_if(abandoned_, [](std::future<std::uintmax_t>& f) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    if (size_.valid() && !size_ready_) {
        abandoned_.push_back(std::move(size_));
    }
    index_ = game_index;
    focus_first_ = true;
    size_ready_ = false;
    size_bytes_ = 0;
    size_ = std::async(std::launch::async, FolderSize, app.library.Games()[game_index].root);
}

bool GameInfoPanel::Draw(App& app, ImVec2 size) {
    auto& games = app.library.Games();
    if (index_ < 0 || index_ >= static_cast<int>(games.size())) {
        return false;
    }
    Game& game = games[index_];

    if (!size_ready_ && size_.valid() &&
        size_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        size_bytes_ = size_.get();
        size_ready_ = true;
    }

    const float s = size.y / 1080.0f;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 5, 20, 150));
    const ImVec2 panel(std::min(1100.0f * s, size.x - 40.0f), size.y * 0.8f);
    ImGui::SetNextWindowPos(ImVec2((size.x - panel.x) / 2, (size.y - panel.y) / 2));
    ImGui::SetNextWindowSize(panel);

    bool open = true;
    ImGui::Begin("##game_info", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);

    const float icon = 280.0f * s;
    const float button_bar = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y;
    if (!ImGui::BeginTable("##layout", 2, ImGuiTableFlags_None,
                           ImVec2(0, panel.y - button_bar - ImGui::GetStyle().WindowPadding.y))) {
        ImGui::End();
        return true;
    }
    ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, icon + 24 * s);
    ImGui::TableSetupColumn("details", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    const ImVec2 icon_pos = ImGui::GetCursorScreenPos();
    if (game.icon) {
        ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(game.icon.get())),
                     ImVec2(icon, icon));
    } else {
        ImGui::Dummy(ImVec2(icon, icon));
        ImGui::GetWindowDrawList()->AddRectFilled(icon_pos, ImVec2(icon_pos.x + icon, icon_pos.y + icon),
                                                  IM_COL32(30, 70, 150, 255), 4 * s);
    }

    // Statut de compatibilité sous l'icône.
    const CompatEntry* compat = app.compat.Find(game.title_id);
    const CompatStatus status = compat ? compat->status : CompatStatus::Unknown;
    ImGui::Spacing();
    const ImVec2 pill = ImGui::GetCursorScreenPos();
    const char* label = Compatibility::Label(status);
    const ImVec2 lsz = ImGui::CalcTextSize(label);
    ImGui::GetWindowDrawList()->AddRectFilled(
        pill, ImVec2(pill.x + icon, pill.y + lsz.y + 14 * s), Compatibility::Color(status), 6 * s);
    ImGui::GetWindowDrawList()->AddText(ImVec2(pill.x + (icon - lsz.x) / 2, pill.y + 7 * s),
                                        IM_COL32(15, 20, 30, 255), label);
    ImGui::Dummy(ImVec2(icon, lsz.y + 14 * s));

    ImGui::TableSetColumnIndex(1);
    ImGui::PushFont(app.fonts.title);
    ImGui::TextUnformatted(game.title.c_str());
    ImGui::PopFont();

    if (ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 220 * s);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        Row("Identifiant", game.title_id);
        Row("Catégorie", game.category);
        Row("Version", game.version);
        Row("Mise à jour", game.update_version.empty() ? "Aucune" : game.update_version);
        Row("Mods", game.has_mods ? "Dossier -mods détecté" : "Aucun");
        Row("Firmware requis", game.required_firmware);
        if (const GameProfile* profile = FindProfile(game.title_id)) {
            Row("Réglages shadPS4",
                std::string(profile->summary) +
                    (app.settings.game_profiles ? "" : " (désactivé dans les Réglages)"));
        }
        Row("Taille", size_ready_ ? FormatSize(size_bytes_) : "Calcul en cours...");
        Row("Emplacement", ToUtf8(game.root));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::PushFont(app.fonts.large);
    ImGui::TextUnformatted("Compatibilité avec shadPS4");
    ImGui::PopFont();
    ImGui::TextWrapped("%s", Compatibility::Description(status));
    if (compat && !compat->tested_version.empty()) {
        ImGui::TextDisabled("Dernier test : shadPS4 %s, le %s", compat->tested_version.c_str(),
                            compat->last_tested.c_str());
    } else if (!app.compat.Loaded()) {
        ImGui::TextDisabled("Base de compatibilité pas encore téléchargée (connexion requise).");
    }
    ImGui::EndTable();

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() -
                         ImGui::GetStyle().WindowPadding.y);
    if (focus_first_) {
        ImGui::SetKeyboardFocusHere();
        focus_first_ = false;
    }
    if (ImGui::Button("Démarrer")) {
        open = false;
        app.LaunchGame(game);
    }
    ImGui::SameLine();
    if (ImGui::Button("Ouvrir le dossier")) {
        SDL_OpenURL(("file://" + ToUtf8(game.root)).c_str());
    }
    if (compat && !compat->issue_url.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Rapport de compatibilité")) {
            SDL_OpenURL(compat->issue_url.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Fermer") || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) {
        open = false;
    }
    ImGui::End();
    return open;
}
