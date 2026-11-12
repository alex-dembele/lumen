#pragma once

#include <filesystem>
#include <vector>

#include <imgui.h>

#include "textures.h"

class App;

// Écran d'accueil inspiré de la PS4 : rangée horizontale de tuiles, la tuile choisie
// est agrandie, son illustration remplit le fond et son titre s'affiche en dessous.
class HomeScreen {
public:
    void Draw(App& app, ImDrawList* dl, ImVec2 size, float dt);

    void Move(int delta, int tile_count);
    void Confirm(App& app);
    void SetSelected(int index) { selected_ = index; }
    int Selected() const { return selected_; }

    bool IsAnimating() const { return animating_; }

private:
    void UpdateBackground(App& app);
    void DrawBackground(App& app, ImDrawList* dl, ImVec2 size, float s);
    void DrawTopBar(App& app, ImDrawList* dl, ImVec2 size, float s);
    void DrawTiles(App& app, ImDrawList* dl, ImVec2 size, float s, float k);
    void DrawHints(App& app, ImDrawList* dl, ImVec2 size, float s);

    int selected_ = 0;
    float scroll_ = 0.0f;
    std::vector<float> tile_focus_;
    float time_ = 0.0f;
    bool animating_ = true;

    std::filesystem::path bg_path_;
    TexturePtr bg_current_;
    TexturePtr bg_previous_;
    float bg_fade_ = 1.0f;
};
