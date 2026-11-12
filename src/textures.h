#pragma once

#include <filesystem>
#include <memory>

#include <SDL3/SDL.h>

struct TextureDeleter {
    void operator()(SDL_Texture* t) const { SDL_DestroyTexture(t); }
};
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;

// Charge une image (PNG/JPG) en texture. Les images plus grandes que max_size sont
// réduites par divisions successives par deux pour économiser la mémoire vidéo.
TexturePtr LoadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, int max_size);

bool SaveScreenshot(SDL_Renderer* renderer, const std::filesystem::path& path);
