#include "ui/theme.h"

#include <cfloat>
#include <filesystem>

namespace Theme {

namespace {

float g_atlas_scale = 1.0f;

// Polices système proches de la police fine de l'interface PS4 (pas de police embarquée).
const char* const kLightFonts[] = {
    "C:/Windows/Fonts/segoeuil.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Light.ttf",
    "/usr/share/fonts/noto/NotoSans-Light.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-ExtraLight.ttf",
};

const char* const kRegularFonts[] = {
    "C:/Windows/Fonts/segoeui.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
};

template <std::size_t N>
const char* FirstExisting(const char* const (&paths)[N]) {
    std::error_code ec;
    for (const char* p : paths) {
        if (std::filesystem::is_regular_file(p, ec)) {
            return p;
        }
    }
    return nullptr;
}

ImFont* AddFont(const char* path, float size) {
    ImGuiIO& io = ImGui::GetIO();
    if (!path) {
        ImFontConfig cfg;
        cfg.SizePixels = size;
        return io.Fonts->AddFontDefault(&cfg);
    }
    return io.Fonts->AddFontFromFileTTF(path, size, nullptr, io.Fonts->GetGlyphRangesDefault());
}

} // namespace

Fonts LoadFonts(float atlas_scale) {
    g_atlas_scale = atlas_scale;
    const float scale = atlas_scale;
    const char* light = FirstExisting(kLightFonts);
    const char* regular = FirstExisting(kRegularFonts);
    if (!light) {
        light = regular;
    }

    Fonts fonts;
    fonts.body = AddFont(regular, 20.0f * scale);
    fonts.small = AddFont(regular, 16.0f * scale);
    fonts.title = AddFont(light, 38.0f * scale);
    fonts.large = AddFont(light, 26.0f * scale);
    return fonts;
}

void ApplyImGuiStyle(float scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(24, 20);
    style.FramePadding = ImVec2(12, 8);
    style.ItemSpacing = ImVec2(12, 12);
    style.WindowBorderSize = 0.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.03f, 0.08f, 0.20f, 0.94f);
    c[ImGuiCol_PopupBg] = ImVec4(0.03f, 0.08f, 0.20f, 0.98f);
    c[ImGuiCol_FrameBg] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
    c[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.22f);
    c[ImGuiCol_Button] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.44f, 0.80f, 0.85f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.44f, 0.80f, 1.0f);
    c[ImGuiCol_CheckMark] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.0f, 0.44f, 0.80f, 0.55f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.44f, 0.80f, 0.80f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.44f, 0.80f, 1.0f);
    c[ImGuiCol_NavCursor] = ImVec4(1.0f, 1.0f, 1.0f, 0.9f);
    c[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.15f);
    c[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.75f, 0.82f, 0.92f, 0.7f);

    style.ScaleAllSizes(scale);
}

void DrawPadButton(ImDrawList* dl, ImVec2 c, float r, PadButton button) {
    dl->AddCircleFilled(c, r, IM_COL32(20, 20, 30, 200));
    const float s = r * 0.48f;
    const float t = r * 0.14f;
    switch (button) {
    case PadButton::Cross: {
        const ImU32 col = IM_COL32(124, 178, 232, 255);
        dl->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, t);
        dl->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), col, t);
        break;
    }
    case PadButton::Circle:
        dl->AddCircle(c, s, IM_COL32(255, 102, 102, 255), 0, t);
        break;
    case PadButton::Triangle:
        dl->AddTriangle(ImVec2(c.x, c.y - s), ImVec2(c.x + s, c.y + s * 0.75f),
                        ImVec2(c.x - s, c.y + s * 0.75f), IM_COL32(64, 226, 160, 255), t);
        break;
    case PadButton::Square:
        dl->AddRect(ImVec2(c.x - s * 0.85f, c.y - s * 0.85f), ImVec2(c.x + s * 0.85f, c.y + s * 0.85f),
                    IM_COL32(255, 105, 248, 255), 0.0f, 0, t);
        break;
    }
}

float FontPx(ImFont* font, float s) {
    return font->FontSize / g_atlas_scale * s;
}

void DrawLabel(ImDrawList* dl, ImFont* font, float s, ImVec2 pos, ImU32 col, const char* text) {
    dl->AddText(font, FontPx(font, s), pos, col, text);
}

void DrawTextShadow(ImDrawList* dl, ImFont* font, float s, ImVec2 pos, ImU32 col, const char* text) {
    const float size = FontPx(font, s);
    dl->AddText(font, size, ImVec2(pos.x + s, pos.y + 2 * s), IM_COL32(0, 0, 0, 90), text);
    dl->AddText(font, size, pos, col, text);
}

ImVec2 TextSize(ImFont* font, float s, const char* text) {
    return font->CalcTextSizeA(FontPx(font, s), FLT_MAX, 0.0f, text);
}

} // namespace Theme
