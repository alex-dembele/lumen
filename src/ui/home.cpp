#include "ui/home.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

#include "app.h"
#include "ui/theme.h"

namespace {

constexpr int kBackgroundMaxSize = 1280;

ImTextureID ToImTexture(SDL_Texture* tex) {
    return static_cast<ImTextureID>(reinterpret_cast<intptr_t>(tex));
}

ImU32 WithAlpha(ImU32 col, float alpha) {
    const auto a = static_cast<int>(((col >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(alpha, 0.0f, 1.0f));
    return (col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
}

void DrawImageCover(ImDrawList* dl, SDL_Texture* tex, ImVec2 p0, ImVec2 p1, float alpha) {
    float tw = 0.0f;
    float th = 0.0f;
    SDL_GetTextureSize(tex, &tw, &th);
    if (tw <= 0 || th <= 0) {
        return;
    }
    const float target = (p1.x - p0.x) / (p1.y - p0.y);
    const float source = tw / th;
    ImVec2 uv0(0, 0);
    ImVec2 uv1(1, 1);
    if (source > target) {
        const float crop = (1.0f - target / source) * 0.5f;
        uv0.x = crop;
        uv1.x = 1.0f - crop;
    } else {
        const float crop = (1.0f - source / target) * 0.5f;
        uv0.y = crop;
        uv1.y = 1.0f - crop;
    }
    // Découpé en tuiles : le rendu logiciel de SDL dessine mal les très grands triangles texturés.
    constexpr int kGrid = 4;
    const ImU32 col = WithAlpha(IM_COL32_WHITE, alpha);
    for (int gy = 0; gy < kGrid; ++gy) {
        for (int gx = 0; gx < kGrid; ++gx) {
            const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
            const float u0 = static_cast<float>(gx) / kGrid;
            const float v0 = static_cast<float>(gy) / kGrid;
            const float u1 = static_cast<float>(gx + 1) / kGrid;
            const float v1 = static_cast<float>(gy + 1) / kGrid;
            dl->AddImage(ToImTexture(tex), ImVec2(lerp(p0.x, p1.x, u0), lerp(p0.y, p1.y, v0)),
                         ImVec2(lerp(p0.x, p1.x, u1), lerp(p0.y, p1.y, v1)),
                         ImVec2(lerp(uv0.x, uv1.x, u0), lerp(uv0.y, uv1.y, v0)),
                         ImVec2(lerp(uv0.x, uv1.x, u1), lerp(uv0.y, uv1.y, v1)), col);
        }
    }
}

// Dégradé radial en un seul maillage : une seule passe de remplissage, contrairement à
// une pile de cercles transparents qui coûte cher aux petits GPU.
void DrawRadialGlow(ImDrawList* dl, ImVec2 center, float radius, ImU32 inner) {
    constexpr int kSegments = 64;
    constexpr float kRings[] = {0.0f, 0.3f, 0.6f, 1.0f};
    constexpr float kAlpha[] = {1.0f, 0.55f, 0.18f, 0.0f};
    constexpr int kRingCount = 4;

    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    const int vtx_count = 1 + kSegments * (kRingCount - 1);
    const int idx_count = kSegments * 3 + kSegments * 6 * (kRingCount - 2);
    dl->PrimReserve(idx_count, vtx_count);
    const auto base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);

    dl->PrimWriteVtx(center, uv, WithAlpha(inner, kAlpha[0]));
    for (int r = 1; r < kRingCount; ++r) {
        for (int i = 0; i < kSegments; ++i) {
            const float a = 2.0f * 3.14159265f * i / kSegments;
            const ImVec2 p(center.x + std::cos(a) * radius * kRings[r],
                           center.y + std::sin(a) * radius * kRings[r]);
            dl->PrimWriteVtx(p, uv, WithAlpha(inner, kAlpha[r]));
        }
    }
    const auto ring = [&](int r, int i) {
        return static_cast<ImDrawIdx>(base + 1 + (r - 1) * kSegments + (i % kSegments));
    };
    for (int i = 0; i < kSegments; ++i) {
        dl->PrimWriteIdx(base);
        dl->PrimWriteIdx(ring(1, i));
        dl->PrimWriteIdx(ring(1, i + 1));
    }
    for (int r = 1; r < kRingCount - 1; ++r) {
        for (int i = 0; i < kSegments; ++i) {
            dl->PrimWriteIdx(ring(r, i));
            dl->PrimWriteIdx(ring(r + 1, i));
            dl->PrimWriteIdx(ring(r + 1, i + 1));
            dl->PrimWriteIdx(ring(r, i));
            dl->PrimWriteIdx(ring(r + 1, i + 1));
            dl->PrimWriteIdx(ring(r, i + 1));
        }
    }
}

// Petit générateur déterministe pour placer les particules du fond.
float Hash(int i) {
    const float x = std::sin(static_cast<float>(i) * 12.9898f) * 43758.5453f;
    return x - std::floor(x);
}

std::string Initials(const std::string& title) {
    std::string out;
    bool word_start = true;
    for (const char c : title) {
        if (c == ' ') {
            word_start = true;
        } else if (word_start && out.size() < 2 && static_cast<unsigned char>(c) < 0x80) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            word_start = false;
        } else {
            word_start = false;
        }
    }
    return out.empty() ? "?" : out;
}

} // namespace

void HomeScreen::Move(int delta, int tile_count) {
    selected_ = std::clamp(selected_ + delta, 0, std::max(0, tile_count - 1));
}

void HomeScreen::Confirm(App& app) {
    auto& games = app.library.Games();
    if (selected_ < static_cast<int>(games.size())) {
        app.LaunchGame(games[selected_]);
    } else {
        app.RequestGameFolder();
    }
}

void HomeScreen::Draw(App& app, ImDrawList* dl, ImVec2 size, float dt) {
    const float s = size.y / 1080.0f;
    const float k = 1.0f - std::exp(-dt * 12.0f);
    time_ += dt;
    animating_ = false;

    const int tile_count = static_cast<int>(app.library.Games().size()) + 1;
    selected_ = std::clamp(selected_, 0, tile_count - 1);

    UpdateBackground(app);
    if (bg_fade_ < 1.0f) {
        bg_fade_ = std::min(1.0f, bg_fade_ + dt * 2.5f);
        animating_ = true;
    } else {
        bg_previous_.reset();
    }

    DrawBackground(app, dl, size, s);
    DrawTopBar(app, dl, size, s);
    DrawTiles(app, dl, size, s, k);
    DrawHints(app, dl, size, s);
}

void HomeScreen::UpdateBackground(App& app) {
    auto& games = app.library.Games();
    const std::filesystem::path wanted =
        selected_ < static_cast<int>(games.size()) ? games[selected_].background_path
                                                   : std::filesystem::path{};
    if (wanted == bg_path_) {
        return;
    }
    bg_path_ = wanted;
    bg_previous_ = std::move(bg_current_);
    bg_current_ = wanted.empty() ? nullptr : LoadTexture(app.renderer, wanted, kBackgroundMaxSize);
    bg_fade_ = 0.0f;
}

void HomeScreen::DrawBackground(App& app, ImDrawList* dl, ImVec2 size, float s) {
    dl->AddRectFilledMultiColor(ImVec2(0, 0), size, Theme::kBgTop, Theme::kBgTop,
                                Theme::kBgBottom, Theme::kBgBottom);

    // Halo lumineux en haut à droite, comme le thème par défaut de la PS4.
    const ImVec2 glow(size.x * 0.78f, size.y * 0.12f);
    DrawRadialGlow(dl, glow, size.y * 0.75f, IM_COL32(90, 170, 255, 70));

    if (bg_previous_) {
        DrawImageCover(dl, bg_previous_.get(), ImVec2(-1, -1), ImVec2(size.x + 1, size.y + 1),
                       1.0f - bg_fade_);
    }
    if (bg_current_) {
        DrawImageCover(dl, bg_current_.get(), ImVec2(-1, -1), ImVec2(size.x + 1, size.y + 1),
                       bg_fade_);
    }
    if (bg_previous_ || bg_current_) {
        dl->AddRectFilledMultiColor(ImVec2(0, 0), size, IM_COL32(0, 10, 30, 110),
                                    IM_COL32(0, 10, 30, 110), IM_COL32(0, 10, 30, 220),
                                    IM_COL32(0, 10, 30, 220));
    }

    const float t = app.settings.animations ? time_ : 0.0f;
    if (app.settings.animations) {
        animating_ = true;
    }

    constexpr int kPoints = 72;
    ImVec2 pts[kPoints];
    for (int w = 0; w < 5; ++w) {
        const float base = size.y * (0.66f + 0.045f * w);
        const float amp = size.y * (0.035f + 0.01f * w);
        const float phase = w * 1.7f;
        for (int layer = 0; layer < 3; ++layer) {
            for (int i = 0; i < kPoints; ++i) {
                const float x = size.x * i / (kPoints - 1);
                const float u = x / size.x;
                const float y = base + layer * 3.0f * s +
                                std::sin(u * 5.5f + t * (0.25f + w * 0.05f) + phase) * amp +
                                std::sin(u * 11.0f - t * 0.18f + phase * 2.0f) * amp * 0.35f;
                pts[i] = ImVec2(x, y);
            }
            const int alpha = layer == 0 ? 70 - w * 8 : 22 - w * 3;
            dl->AddPolyline(pts, kPoints, IM_COL32(170, 215, 255, alpha), 0,
                            (layer == 0 ? 2.0f : 6.0f) * s);
        }
    }

    for (int i = 0; i < 48; ++i) {
        const float px = Hash(i) * size.x;
        const float speed = 8.0f + Hash(i + 100) * 18.0f;
        const float py = std::fmod(Hash(i + 200) * size.y - t * speed * s + size.y * 4, size.y);
        const float twinkle = 0.5f + 0.5f * std::sin(t * (0.8f + Hash(i + 300)) + i);
        dl->AddCircleFilled(ImVec2(px, py), (1.0f + Hash(i + 400) * 1.8f) * s,
                            IM_COL32(200, 230, 255, static_cast<int>(25 + 70 * twinkle)), 8);
    }
}

void HomeScreen::DrawTopBar(App& app, ImDrawList* dl, ImVec2 size, float s) {
    const ImVec2 avatar(80 * s, 56 * s);
    const float r = 22 * s;
    dl->AddCircleFilled(avatar, r, Theme::kAccent, 32);
    dl->AddCircle(avatar, r, IM_COL32(255, 255, 255, 160), 32, 2 * s);
    const std::string initial = Initials(app.settings.user_name).substr(0, 1);
    const ImVec2 isz = Theme::TextSize(app.fonts.body, s, initial.c_str());
    Theme::DrawLabel(dl, app.fonts.body, s,
                ImVec2(avatar.x - isz.x / 2, avatar.y - isz.y / 2), Theme::kText, initial.c_str());
    Theme::DrawTextShadow(dl, app.fonts.body, s,
                          ImVec2(avatar.x + r + 14 * s, avatar.y - Theme::FontPx(app.fonts.body, s) / 2),
                          Theme::kText, app.settings.user_name.c_str());

    SDL_Time now = 0;
    SDL_DateTime dt{};
    char clock[16] = "--:--";
    if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &dt, true)) {
        std::snprintf(clock, sizeof(clock), "%02d:%02d", dt.hour, dt.minute);
    }
    const ImVec2 csz = Theme::TextSize(app.fonts.large, s, clock);
    Theme::DrawTextShadow(dl, app.fonts.large, s,
                          ImVec2(size.x - 80 * s - csz.x, avatar.y - csz.y / 2), Theme::kText,
                          clock);
}

void HomeScreen::DrawTiles(App& app, ImDrawList* dl, ImVec2 size, float s, float k) {
    auto& games = app.library.Games();
    const int count = static_cast<int>(games.size()) + 1;
    tile_focus_.resize(count, 0.0f);

    const float small = 150 * s;
    const float big = 236 * s;
    const float gap = 16 * s;
    const float base_x = 110 * s;
    const float top = size.y * 0.25f;

    const float target_scroll = selected_ * (small + gap);
    scroll_ += (target_scroll - scroll_) * k;
    if (std::abs(target_scroll - scroll_) > 0.5f) {
        animating_ = true;
    }

    float x = base_x - scroll_;
    ImVec2 selected_pos;
    float selected_size = big;

    for (int i = 0; i < count; ++i) {
        const float goal = i == selected_ ? 1.0f : 0.0f;
        tile_focus_[i] += (goal - tile_focus_[i]) * k;
        if (std::abs(goal - tile_focus_[i]) > 0.002f) {
            animating_ = true;
        }
        const float f = tile_focus_[i];
        const float w = small + (big - small) * f;
        const ImVec2 p0(x, top);
        const ImVec2 p1(x + w, top + w);
        x += w + gap;

        if (p1.x < -big || p0.x > size.x + big) {
            continue;
        }
        const float alpha = p0.x < base_x - 1.0f
                                ? std::clamp(1.0f - (base_x - p0.x) / (small * 1.5f), 0.0f, 1.0f)
                                : 1.0f;
        if (alpha <= 0.0f) {
            continue;
        }

        if (f > 0.01f) {
            for (int g = 1; g <= 8; ++g) {
                const float e = g * 2.5f * s;
                dl->AddRect(ImVec2(p0.x - e, p0.y - e), ImVec2(p1.x + e, p1.y + e),
                            IM_COL32(120, 190, 255, static_cast<int>(28 * f * alpha / g * 3)),
                            6 * s, 0, 2.5f * s);
            }
        }

        const float rounding = 4 * s;
        const bool is_game = i < static_cast<int>(games.size());
        if (is_game && games[i].icon) {
            dl->AddImageRounded(ToImTexture(games[i].icon.get()), p0, p1, ImVec2(0, 0),
                                ImVec2(1, 1), WithAlpha(IM_COL32_WHITE, alpha), rounding);
        } else if (is_game) {
            dl->AddRectFilledMultiColor(p0, p1, WithAlpha(IM_COL32(40, 90, 170, 255), alpha),
                                        WithAlpha(IM_COL32(20, 60, 140, 255), alpha),
                                        WithAlpha(IM_COL32(10, 30, 80, 255), alpha),
                                        WithAlpha(IM_COL32(20, 50, 120, 255), alpha));
            const std::string ini = Initials(games[i].title);
            const ImVec2 tsz = Theme::TextSize(app.fonts.title, s, ini.c_str());
            Theme::DrawLabel(dl, app.fonts.title, s,
                        ImVec2((p0.x + p1.x - tsz.x) / 2, (p0.y + p1.y - tsz.y) / 2),
                        WithAlpha(Theme::kText, alpha), ini.c_str());
        } else {
            dl->AddRectFilled(p0, p1, WithAlpha(IM_COL32(255, 255, 255, 30), alpha), rounding);
            const ImVec2 c((p0.x + p1.x) / 2, (p0.y + p1.y) / 2);
            const float arm = w * 0.14f;
            const ImU32 col = WithAlpha(IM_COL32(255, 255, 255, 220), alpha);
            dl->AddLine(ImVec2(c.x - arm, c.y), ImVec2(c.x + arm, c.y), col, 4 * s);
            dl->AddLine(ImVec2(c.x, c.y - arm), ImVec2(c.x, c.y + arm), col, 4 * s);
        }
        dl->AddRect(p0, p1, WithAlpha(IM_COL32(255, 255, 255, static_cast<int>(40 + 200 * f)), alpha),
                    rounding, 0, (1.0f + 2.0f * f) * s);

        if (i == selected_) {
            selected_pos = p0;
            selected_size = w;
        }
    }

    // Détails de l'élément sélectionné, sous la tuile.
    const ImVec2 text_pos(selected_pos.x, top + selected_size + 28 * s);
    const bool is_game = selected_ < static_cast<int>(games.size());
    const std::string title = is_game ? games[selected_].title : "Ajouter des jeux";
    Theme::DrawTextShadow(dl, app.fonts.title, s, text_pos, Theme::kText, title.c_str());

    std::string info;
    if (is_game) {
        info = games[selected_].title_id;
        if (!games[selected_].version.empty()) {
            info += (info.empty() ? "" : "   ·   ") + games[selected_].version;
        }
    } else {
        info = "Choisissez un dossier ou glissez-le dans cette fenêtre";
    }
    const float info_y = text_pos.y + Theme::FontPx(app.fonts.title, s) + 8 * s;
    Theme::DrawTextShadow(dl, app.fonts.body, s, ImVec2(text_pos.x, info_y), Theme::kTextDim,
                          info.c_str());

    // Pastille de compatibilité shadPS4 à droite des infos.
    if (is_game && app.compat.Loaded()) {
        const CompatEntry* entry = app.compat.Find(games[selected_].title_id);
        const CompatStatus status = entry ? entry->status : CompatStatus::Unknown;
        const float x0 = text_pos.x + Theme::TextSize(app.fonts.body, s, info.c_str()).x + 28 * s;
        const float cy = info_y + Theme::FontPx(app.fonts.body, s) / 2;
        dl->AddCircleFilled(ImVec2(x0, cy), 6 * s, Compatibility::Color(status), 16);
        Theme::DrawTextShadow(dl, app.fonts.body, s, ImVec2(x0 + 14 * s, info_y),
                              Compatibility::Color(status), Compatibility::Label(status));
    }

    const char* label = is_game ? "Démarrer" : "Parcourir";
    const ImVec2 lsz = Theme::TextSize(app.fonts.body, s, label);
    const ImVec2 b0(text_pos.x, info_y + Theme::FontPx(app.fonts.body, s) + 26 * s);
    const ImVec2 b1(b0.x + lsz.x + 84 * s, b0.y + 52 * s);
    dl->AddRectFilled(b0, b1, IM_COL32(255, 255, 255, 235), 4 * s);
    Theme::DrawPadButton(dl, ImVec2(b0.x + 30 * s, (b0.y + b1.y) / 2), 14 * s,
                         Theme::PadButton::Cross);
    Theme::DrawLabel(dl, app.fonts.body, s,
                ImVec2(b0.x + 56 * s, (b0.y + b1.y - lsz.y) / 2), IM_COL32(20, 30, 50, 255),
                label);
}

void HomeScreen::DrawHints(App& app, ImDrawList* dl, ImVec2 size, float s) {
    struct Hint {
        Theme::PadButton button;
        const char* text;
    };
    const Hint hints[] = {
        {Theme::PadButton::Cross, "Valider  (Entrée)"},
        {Theme::PadButton::Triangle, "Infos  (I)"},
        {Theme::PadButton::Options, "Réglages  (Tab)"},
    };

    float x = size.x - 80 * s;
    const float y = size.y - 56 * s;
    for (int i = static_cast<int>(std::size(hints)) - 1; i >= 0; --i) {
        const ImVec2 tsz = Theme::TextSize(app.fonts.small, s, hints[i].text);
        x -= tsz.x;
        Theme::DrawTextShadow(dl, app.fonts.small, s, ImVec2(x, y - tsz.y / 2), Theme::kTextDim,
                              hints[i].text);
        x -= 24 * s;
        Theme::DrawPadButton(dl, ImVec2(x, y), 13 * s, hints[i].button);
        x -= 44 * s;
    }

    const std::size_t n = app.library.Games().size();
    char count[64];
    std::snprintf(count, sizeof(count), "%zu %s dans la bibliothèque", n, n > 1 ? "jeux" : "jeu");
    Theme::DrawTextShadow(dl, app.fonts.small, s,
                          ImVec2(80 * s, y - Theme::FontPx(app.fonts.small, s) / 2), Theme::kTextDim,
                          count);
}
