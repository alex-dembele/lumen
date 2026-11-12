#include "audio.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>


namespace {

constexpr int kSfxRate = 48000;
constexpr float kPi = 3.14159265f;
constexpr float kMusicVolume = 0.55f;
constexpr float kFadeSeconds = 0.6f;
constexpr int kMusicMaxSeconds = 120;

// Note brève à attaque rapide et décroissance exponentielle, comme un « clic » de menu.
void AddTone(std::vector<float>& buf, float start, float freq, float duration, float amp,
             float decay) {
    const int first = static_cast<int>(start * kSfxRate);
    const int count = static_cast<int>(duration * kSfxRate);
    if (buf.size() < static_cast<std::size_t>(first + count)) {
        buf.resize(first + count, 0.0f);
    }
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / kSfxRate;
        const float attack = std::min(1.0f, t * 400.0f);
        const float env = attack * std::exp(-t * decay);
        buf[first + i] += amp * env *
                          (std::sin(2 * kPi * freq * t) + 0.25f * std::sin(4 * kPi * freq * t));
    }
}

std::vector<float> Synthesize(Sfx sfx) {
    std::vector<float> buf;
    switch (sfx) {
    case Sfx::Move:
        AddTone(buf, 0.0f, 1480.0f, 0.05f, 0.12f, 110.0f);
        break;
    case Sfx::Confirm:
        AddTone(buf, 0.0f, 1046.5f, 0.18f, 0.14f, 22.0f);
        AddTone(buf, 0.07f, 1568.0f, 0.25f, 0.12f, 16.0f);
        break;
    case Sfx::Back:
        AddTone(buf, 0.0f, 784.0f, 0.15f, 0.12f, 26.0f);
        AddTone(buf, 0.06f, 523.3f, 0.2f, 0.10f, 22.0f);
        break;
    case Sfx::Open: {
        const int count = static_cast<int>(0.28f * kSfxRate);
        buf.resize(count);
        float phase = 0.0f;
        for (int i = 0; i < count; ++i) {
            const float u = static_cast<float>(i) / count;
            phase += 2 * kPi * (420.0f + 520.0f * u) / kSfxRate;
            buf[i] = 0.09f * std::sin(kPi * u) * std::sin(phase);
        }
        break;
    }
    case Sfx::Count:
        break;
    }
    return buf;
}

} // namespace

struct Audio::DecodeState {
    std::mutex mutex;
    int generation = 0;
    std::shared_ptr<const PcmAudio> ready;
    int ready_generation = -1;
};

Audio::~Audio() {
    Shutdown();
}

bool Audio::Init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return false;
    }
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!device_) {
        return false;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, kSfxRate};
    for (std::size_t i = 0; i < sfx_streams_.size(); ++i) {
        sfx_data_[i] = Synthesize(static_cast<Sfx>(i));
        sfx_streams_[i] = SDL_CreateAudioStream(&spec, nullptr);
        SDL_BindAudioStream(device_, sfx_streams_[i]);
    }
    decode_ = std::make_shared<DecodeState>();
    return true;
}

void Audio::Shutdown() {
    for (auto& stream : sfx_streams_) {
        if (stream) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
    }
    if (music_) {
        SDL_DestroyAudioStream(music_);
        music_ = nullptr;
    }
    if (device_) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
}

void Audio::Play(Sfx sfx) {
    const auto i = static_cast<std::size_t>(sfx);
    if (!device_ || !sfx_enabled || !sfx_streams_[i]) {
        return;
    }
    SDL_ClearAudioStream(sfx_streams_[i]);
    SDL_PutAudioStreamData(sfx_streams_[i], sfx_data_[i].data(),
                           static_cast<int>(sfx_data_[i].size() * sizeof(float)));
}

void Audio::PlayMusic(const std::filesystem::path& path) {
    const std::filesystem::path wanted = music_enabled ? path : std::filesystem::path{};
    if (!device_ || wanted == music_path_) {
        return;
    }
    music_path_ = wanted;
    target_gain_ = 0.0f;

    int generation = 0;
    {
        std::lock_guard lock(decode_->mutex);
        generation = ++decode_->generation;
        decode_->ready.reset();
    }
    if (wanted.empty()) {
        return;
    }
    // Le thread ne touche qu'à l'état partagé : il peut survivre à l'objet Audio sans risque.
    std::thread([state = decode_, wanted, generation] {
        auto pcm = std::make_shared<PcmAudio>();
        if (!DecodeAt9(wanted, *pcm, kMusicMaxSeconds)) {
            return;
        }
        std::lock_guard lock(state->mutex);
        if (state->generation == generation) {
            state->ready = std::move(pcm);
            state->ready_generation = generation;
        }
    }).detach();
}

void Audio::Update(float dt) {
    if (!device_) {
        return;
    }
    if (!music_enabled && !music_path_.empty()) {
        PlayMusic({});
    }

    const float step = dt / kFadeSeconds;
    gain_ = gain_ < target_gain_ ? std::min(target_gain_, gain_ + step)
                                 : std::max(target_gain_, gain_ - step);
    if (music_) {
        SDL_SetAudioStreamGain(music_, gain_ * kMusicVolume);
    }

    if (gain_ <= 0.0f) {
        StartMusicStream();
    }

    // Relance la musique en boucle avant que le tampon ne se vide.
    if (music_ && current_) {
        const int one_second = current_->sample_rate * current_->channels * 2;
        if (SDL_GetAudioStreamQueued(music_) < one_second) {
            SDL_PutAudioStreamData(music_, current_->samples.data(),
                                   static_cast<int>(current_->samples.size() * 2));
        }
    }
}

void Audio::StartMusicStream() {
    std::shared_ptr<const PcmAudio> next;
    {
        std::lock_guard lock(decode_->mutex);
        if (decode_->ready && decode_->ready_generation == decode_->generation) {
            next = std::move(decode_->ready);
        }
    }
    if (!next) {
        if (music_path_.empty() && music_) {
            SDL_ClearAudioStream(music_);
            current_.reset();
        }
        return;
    }

    if (music_) {
        SDL_DestroyAudioStream(music_);
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, next->channels, next->sample_rate};
    music_ = SDL_CreateAudioStream(&spec, nullptr);
    if (!music_) {
        return;
    }
    SDL_SetAudioStreamGain(music_, 0.0f);
    SDL_BindAudioStream(device_, music_);
    current_ = std::move(next);
    target_gain_ = 1.0f;
}
