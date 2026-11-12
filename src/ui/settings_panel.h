#pragma once

#include <imgui.h>

class App;

class SettingsPanel {
public:
    void Open(App& app);
    // Renvoie false quand l'utilisateur ferme le panneau.
    bool Draw(App& app, ImVec2 size);

private:
    char user_name_[64] = {};
    bool focus_first_ = false;
};
