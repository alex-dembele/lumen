#pragma once

#include <imgui.h>

namespace Theme {

struct Fonts {
    ImFont* body = nullptr;
    ImFont* small = nullptr;
    ImFont* title = nullptr;
    ImFont* large = nullptr;
};

inline constexpr ImU32 kBgTop = IM_COL32(6, 22, 58, 255);
inline constexpr ImU32 kBgBottom = IM_COL32(22, 78, 150, 255);
inline constexpr ImU32 kText = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 kTextDim = IM_COL32(205, 220, 240, 200);
inline constexpr ImU32 kAccent = IM_COL32(0, 112, 204, 255);

enum class PadButton { Cross, Circle, Triangle, Square, Options };

// Les polices sont rastérisées pour un écran 1080p multiplié par atlas_scale, puis
// dessinées à la taille de la fenêtre courante (s = hauteur / 1080).
Fonts LoadFonts(float atlas_scale);
float FontPx(ImFont* font, float s);
void ApplyImGuiStyle(float scale);

void DrawPadButton(ImDrawList* dl, ImVec2 center, float radius, PadButton button);
void DrawLabel(ImDrawList* dl, ImFont* font, float s, ImVec2 pos, ImU32 col, const char* text);
void DrawTextShadow(ImDrawList* dl, ImFont* font, float s, ImVec2 pos, ImU32 col, const char* text);
ImVec2 TextSize(ImFont* font, float s, const char* text);

} // namespace Theme
