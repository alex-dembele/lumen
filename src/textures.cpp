#include "textures.h"

#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace {

std::vector<unsigned char> HalveRgba(const unsigned char* src, int w, int h) {
    const int nw = w / 2;
    const int nh = h / 2;
    std::vector<unsigned char> dst(static_cast<std::size_t>(nw) * nh * 4);
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            for (int c = 0; c < 4; ++c) {
                const auto at = [&](int sx, int sy) {
                    return src[(static_cast<std::size_t>(sy) * w + sx) * 4 + c];
                };
                const int sum = at(x * 2, y * 2) + at(x * 2 + 1, y * 2) + at(x * 2, y * 2 + 1) +
                                at(x * 2 + 1, y * 2 + 1);
                dst[(static_cast<std::size_t>(y) * nw + x) * 4 + c] =
                    static_cast<unsigned char>(sum / 4);
            }
        }
    }
    return dst;
}

} // namespace

TexturePtr LoadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, int max_size) {
    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* raw = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!raw) {
        return nullptr;
    }

    std::vector<unsigned char> pixels(raw, raw + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(raw);

    while ((w > max_size || h > max_size) && w >= 2 && h >= 2) {
        pixels = HalveRgba(pixels.data(), w, h);
        w /= 2;
        h /= 2;
    }

    SDL_Texture* tex =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex) {
        return nullptr;
    }
    SDL_UpdateTexture(tex, nullptr, pixels.data(), w * 4);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    return TexturePtr(tex);
}

bool SaveScreenshot(SDL_Renderer* renderer, const std::filesystem::path& path) {
    SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
    if (!surface) {
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!rgba) {
        return false;
    }
    const bool ok = stbi_write_png(path.string().c_str(), rgba->w, rgba->h, 4, rgba->pixels,
                                   rgba->pitch) != 0;
    SDL_DestroySurface(rgba);
    return ok;
}
