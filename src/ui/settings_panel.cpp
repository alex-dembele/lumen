#include "ui/settings_panel.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "app.h"

namespace {

std::string ToUtf8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

void SectionTitle(App& app, const char* text) {
    ImGui::Spacing();
    ImGui::PushFont(app.fonts.large);
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
    ImGui::Separator();
}

} // namespace

void SettingsPanel::Open(App& app) {
    std::snprintf(user_name_, sizeof(user_name_), "%s", app.settings.user_name.c_str());
    focus_first_ = true;
}

bool SettingsPanel::Draw(App& app, ImVec2 size) {
    const float s = size.y / 1080.0f;
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 5, 20, 150));

    const ImVec2 panel(std::min(900.0f * s, size.x - 40.0f), size.y * 0.82f);
    ImGui::SetNextWindowPos(ImVec2((size.x - panel.x) / 2, (size.y - panel.y) / 2));
    ImGui::SetNextWindowSize(panel);

    bool open = true;
    ImGui::Begin("##settings", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushFont(app.fonts.title);
    ImGui::TextUnformatted("Réglages");
    ImGui::PopFont();

    SectionTitle(app, "Émulateur");
    const std::string emu = app.settings.emulator_path.empty()
                                ? std::string("Non configuré")
                                : ToUtf8(app.settings.emulator_path);
    ImGui::TextDisabled("Programme shadPS4");
    ImGui::TextWrapped("%s", emu.c_str());
    if (focus_first_) {
        ImGui::SetKeyboardFocusHere();
        focus_first_ = false;
    }
    if (ImGui::Button("Choisir shadPS4...")) {
        app.RequestEmulatorPath();
    }
    ImGui::SameLine();
    if (ImGui::Button("Détecter automatiquement")) {
        const auto found = Emulator::FindDefault();
        if (found.empty()) {
            app.ShowToast("shadPS4 introuvable à côté de Lumen ni dans le PATH");
        } else {
            app.settings.emulator_path = found;
            app.SaveSettings();
        }
    }
    bool changed = false;
    changed |= ImGui::Checkbox("Jouer en plein écran", &app.settings.fullscreen);
    changed |= ImGui::Checkbox("Réduire Lumen pendant le jeu", &app.settings.minimize_while_playing);

    SectionTitle(app, "Son");
    changed |= ImGui::Checkbox("Sons de navigation", &app.settings.sounds);
    changed |= ImGui::Checkbox("Musique du jeu sélectionné (snd0)", &app.settings.music);

    SectionTitle(app, "Interface");
    changed |= ImGui::Checkbox("Fond animé (désactiver pour économiser les ressources)",
                               &app.settings.animations);
    ImGui::SetNextItemWidth(320 * s);
    if (ImGui::InputText("Nom du joueur", user_name_, sizeof(user_name_))) {
        app.settings.user_name = user_name_;
        changed = true;
    }

    SectionTitle(app, "Dossiers de jeux");
    int remove_index = -1;
    for (std::size_t i = 0; i < app.settings.game_dirs.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("Retirer")) {
            remove_index = static_cast<int>(i);
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", ToUtf8(app.settings.game_dirs[i]).c_str());
        ImGui::PopID();
    }
    if (app.settings.game_dirs.empty()) {
        ImGui::TextDisabled("Aucun dossier. Un dossier peut être un jeu (CUSAxxxxx) ou en contenir plusieurs.");
    }
    if (remove_index >= 0) {
        app.settings.game_dirs.erase(app.settings.game_dirs.begin() + remove_index);
        app.SaveSettings();
        app.Rescan();
    }
    if (ImGui::Button("Ajouter un dossier...")) {
        app.RequestGameFolder();
    }
    ImGui::SameLine();
    if (ImGui::Button("Actualiser la bibliothèque")) {
        app.Rescan();
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Fermer") || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) {
        open = false;
    }
    ImGui::End();

    if (changed) {
        app.SaveSettings();
    }
    return open;
}
