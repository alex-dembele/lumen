#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <SDL3/SDL.h>

// Lance le cœur shadPS4 dans un processus séparé et surveille sa fin.
class Emulator {
public:
    ~Emulator();

    // Cherche shadPS4 à côté de Lumen puis dans le PATH.
    static std::filesystem::path FindDefault();

    bool Launch(const std::filesystem::path& emulator, const std::filesystem::path& game,
                bool fullscreen, std::string& error);

    bool IsRunning() const { return process_ != nullptr; }

    // Renvoie le code de sortie une fois le jeu fermé.
    std::optional<int> PollExit();

    void Stop();

private:
    SDL_Process* process_ = nullptr;
};
