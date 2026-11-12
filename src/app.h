#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "emulator.h"
#include "library.h"
#include "settings.h"
#include "ui/home.h"
#include "ui/settings_panel.h"
#include "ui/theme.h"

enum class Action { Up, Down, Left, Right, Confirm, Back, Options };

struct LaunchOptions {
    std::filesystem::path config_dir;
    std::filesystem::path screenshot;
    std::string boot_title_id;
    int select = 0;
    bool open_settings = false;
};

class App {
public:
    bool Init(const LaunchOptions& options);
    int Run();
    void Shutdown();

    void LaunchGame(Game& game);
    void Rescan();
    void SaveSettings();
    void ShowToast(std::string message);

    // Les boîtes de dialogue SDL répondent depuis un autre thread : les résultats sont
    // mis en file puis traités dans la boucle principale.
    void RequestGameFolder();
    void RequestEmulatorPath();

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Theme::Fonts fonts;
    float ui_scale = 1.0f;

    Settings settings;
    Library library;
    Emulator emulator;
    std::string running_title;

    bool settings_open = false;
    std::string toast;
    double toast_until = 0.0;

private:
    void HandleEvent(const SDL_Event& event);
    void HandleAction(Action action);
    void HandleStick(float dt);
    void ProcessDialogResults();
    void UpdateRunningGame();
    void DrawFrame(float dt, bool capture);
    void DrawToast(ImDrawList* dl);
    void DrawRunningOverlay(ImDrawList* dl);
    void AddDroppedPath(const std::filesystem::path& path);

    LaunchOptions options_;
    std::filesystem::path settings_file_;
    HomeScreen home_;
    SettingsPanel settings_panel_;
    bool quit_ = false;
    bool imgui_ready_ = false;
    bool capture_ok_ = false;

    std::mutex dialog_mutex_;
    std::vector<std::filesystem::path> pending_game_dirs_;
    std::filesystem::path pending_emulator_;

    float stick_x_ = 0.0f;
    float stick_y_ = 0.0f;
    float stick_repeat_ = 0.0f;
    bool stick_held_ = false;
};
