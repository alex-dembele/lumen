#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <vector>

#include <SDL3/SDL.h>

#include "at9.h"

enum class Sfx { Move, Confirm, Back, Open, Count };

// Sons de l'interface (synthétisés au démarrage, aucun fichier son embarqué) et musique
// du jeu sélectionné (sce_sys/snd0.at9), décodée dans un thread puis jouée en boucle.
class Audio {
public:
    ~Audio();

    bool Init();
    void Shutdown();

    void Play(Sfx sfx);
    // Chemin vide : coupe la musique en fondu.
    void PlayMusic(const std::filesystem::path& path);
    void Update(float dt);
    bool IsFading() const { return gain_ != target_gain_; }

    bool sfx_enabled = true;
    bool music_enabled = true;

private:
    struct DecodeState;

    void StartMusicStream();

    SDL_AudioDeviceID device_ = 0;
    std::array<SDL_AudioStream*, static_cast<std::size_t>(Sfx::Count)> sfx_streams_{};
    std::array<std::vector<float>, static_cast<std::size_t>(Sfx::Count)> sfx_data_;

    SDL_AudioStream* music_ = nullptr;
    std::shared_ptr<DecodeState> decode_;
    std::shared_ptr<const PcmAudio> current_;
    std::filesystem::path music_path_;
    float gain_ = 0.0f;
    float target_gain_ = 0.0f;
};
