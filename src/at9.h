#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct PcmAudio {
    int channels = 0;
    int sample_rate = 0;
    std::vector<std::int16_t> samples; // entrelacés
};

// Décode un fichier ATRAC9 (.at9, ex. sce_sys/snd0.at9) en PCM 16 bits.
// max_seconds limite la mémoire utilisée pour les longues musiques.
bool DecodeAt9(const std::filesystem::path& path, PcmAudio& out, int max_seconds);
