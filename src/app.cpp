#include "app.h"

#include <algorithm>
#include <cmath>

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "profiles.h"

namespace fs = std::filesystem;

namespace {

constexpr float kStickThreshold = 0.55f;
constexpr float kStickFirstRepeat = 0.35f;
constexpr float kStickRepeat = 0.12f;
constexpr double kToastSeconds = 4.0;

double Now() {
    return static_cast<double>(SDL_GetTicks()) / 1000.0;
}

fs::path PathFromUtf8(const char* s) {
    const std::string str(s);
    return fs::path(std::u8string(str.begin(), str.end()));
}

} // namespace

bool App::Init(const LaunchOptions& options) {
    options_ = options;

    if (!options_.screenshot.empty()) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
        return false;
    }

    fs::path config_dir = options_.config_dir;
    if (config_dir.empty()) {
        char* pref = SDL_GetPrefPath("Lumen", "Lumen");
        config_dir = pref ? PathFromUtf8(pref) : fs::current_path();
        SDL_free(pref);
    }
    std::error_code ec;
    fs::create_directories(config_dir, ec);
    settings_file_ = config_dir / "settings.ini";
    settings.Load(settings_file_);
    if (settings.emulator_path.empty()) {
        settings.emulator_path = Emulator::FindDefault();
    }

    const bool screenshot = !options_.screenshot.empty();
    const int width = screenshot ? 1920 : 1280;
    const int height = screenshot ? 1080 : 720;
    window = SDL_CreateWindow("Lumen", width, height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(window, 960, 540);
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    // Polices rastérisées pour la plus grande résolution probable de l'écran.
    float atlas_scale = 1.0f;
    if (const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        atlas_scale = std::clamp(mode->h * mode->pixel_density / 1080.0f, 1.0f, 2.0f);
    }
    ui_scale = atlas_scale;
    fonts = Theme::LoadFonts(atlas_scale);
    io.FontDefault = fonts.body;
    Theme::ApplyImGuiStyle(1.0f);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    imgui_ready_ = true;

    library.Scan(renderer, settings.game_dirs);
    home_.SetSelected(options_.select);
    compat.Init(config_dir);
    if (options_.screenshot.empty()) {
        audio.Init();
    }
    if (!options_.boot_title_id.empty()) {
        auto& games = library.Games();
        const auto it = std::find_if(games.begin(), games.end(), [&](const Game& g) {
            return g.title_id == options_.boot_title_id;
        });
        if (it != games.end()) {
            home_.SetSelected(static_cast<int>(it - games.begin()));
            LaunchGame(*it);
        } else {
            ShowToast("Jeu " + options_.boot_title_id + " introuvable dans la bibliothèque");
        }
    }
    if (options_.open_settings) {
        OpenSettings();
    }
    if (options_.open_info && !library.Games().empty()) {
        info_open = true;
        info_panel_.Open(*this, home_.Selected());
    }
    return true;
}

void App::Shutdown() {
    audio.Shutdown();
    library = Library{};
    home_ = HomeScreen{};
    if (imgui_ready_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

int App::Run() {
    Uint64 last = SDL_GetPerformanceCounter();
    int frame = 0;

    while (!quit_) {
        // Rendu à la demande : on dort tant que rien ne bouge pour rester léger.
        const bool busy = home_.IsAnimating() || settings_open || info_open || stick_held_ ||
                          audio.IsFading() || Now() < toast_until;
        int timeout_ms = busy ? 0 : 500;
        if (emulator.IsRunning()) {
            timeout_ms = 250;
        }

        SDL_Event event;
        if (timeout_ms > 0 && options_.screenshot.empty()) {
            if (SDL_WaitEventTimeout(&event, timeout_ms)) {
                HandleEvent(event);
            }
        }
        while (SDL_PollEvent(&event)) {
            HandleEvent(event);
        }

        const Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - last) / static_cast<float>(SDL_GetPerformanceFrequency());
        last = now;
        dt = std::min(dt, 0.1f);
        if (!options_.screenshot.empty()) {
            dt = 1.0f / 60.0f;
        }

        ProcessDialogResults();
        UpdateRunningGame();
        HandleStick(dt);
        compat.Update();
        UpdateMusic();
        audio.sfx_enabled = settings.sounds;
        audio.music_enabled = settings.music;
        audio.Update(dt);

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            continue;
        }
        const bool capture = !options_.screenshot.empty() && ++frame == 90;
        DrawFrame(dt, capture);
        if (capture) {
            return capture_ok_ ? 0 : 1;
        }
    }
    return 0;
}

void App::DrawFrame(float dt, bool capture) {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 size = io.DisplaySize;
    const float s = size.y / 1080.0f;
    io.FontGlobalScale = s / ui_scale;

    ImGui::NewFrame();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    home_.Draw(*this, dl, size, dt);

    if (settings_open && !settings_panel_.Draw(*this, size)) {
        settings_open = false;
        audio.Play(Sfx::Back);
    }
    if (info_open && !info_panel_.Draw(*this, size)) {
        info_open = false;
        if (!emulator.IsRunning()) {
            audio.Play(Sfx::Back);
        }
    }
    if (emulator.IsRunning()) {
        DrawRunningOverlay(ImGui::GetForegroundDrawList());
    }
    DrawToast(ImGui::GetForegroundDrawList());

    ImGui::Render();
    SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    if (capture) {
        capture_ok_ = SaveScreenshot(renderer, options_.screenshot);
        if (!capture_ok_) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Capture impossible : %s", SDL_GetError());
        }
    }
    SDL_RenderPresent(renderer);
}

void App::HandleEvent(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);

    switch (event.type) {
    case SDL_EVENT_QUIT:
        quit_ = true;
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        SDL_OpenGamepad(event.gdevice.which);
        break;
    case SDL_EVENT_DROP_FILE:
        if (event.drop.data) {
            AddDroppedPath(PathFromUtf8(event.drop.data));
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        switch (event.key.key) {
        case SDLK_LEFT: HandleAction(Action::Left); break;
        case SDLK_RIGHT: HandleAction(Action::Right); break;
        case SDLK_UP: HandleAction(Action::Up); break;
        case SDLK_DOWN: HandleAction(Action::Down); break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: HandleAction(Action::Confirm); break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE: HandleAction(Action::Back); break;
        case SDLK_TAB: HandleAction(Action::Options); break;
        case SDLK_I: HandleAction(Action::Info); break;
        case SDLK_F11:
            SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            break;
        default: break;
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        switch (event.gbutton.button) {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: HandleAction(Action::Left); break;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: HandleAction(Action::Right); break;
        case SDL_GAMEPAD_BUTTON_DPAD_UP: HandleAction(Action::Up); break;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: HandleAction(Action::Down); break;
        case SDL_GAMEPAD_BUTTON_SOUTH: HandleAction(Action::Confirm); break;
        case SDL_GAMEPAD_BUTTON_EAST: HandleAction(Action::Back); break;
        case SDL_GAMEPAD_BUTTON_NORTH: HandleAction(Action::Info); break;
        case SDL_GAMEPAD_BUTTON_START: HandleAction(Action::Options); break;
        default: break;
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            stick_x_ = event.gaxis.value / 32767.0f;
        } else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            stick_y_ = event.gaxis.value / 32767.0f;
        }
        break;
    default:
        break;
    }
}

void App::HandleAction(Action action) {
    if (emulator.IsRunning()) {
        if (action == Action::Back) {
            emulator.Stop();
        }
        return;
    }
    // Les panneaux sont pilotés par la navigation clavier/manette d'ImGui.
    if (settings_open || info_open) {
        return;
    }

    const int tile_count = static_cast<int>(library.Games().size()) + 1;
    const int before = home_.Selected();
    switch (action) {
    case Action::Left: home_.Move(-1, tile_count); break;
    case Action::Right: home_.Move(1, tile_count); break;
    case Action::Confirm:
        audio.Play(Sfx::Confirm);
        home_.Confirm(*this);
        break;
    case Action::Options: OpenSettings(); break;
    case Action::Info:
        if (home_.Selected() < static_cast<int>(library.Games().size())) {
            audio.Play(Sfx::Open);
            info_open = true;
            info_panel_.Open(*this, home_.Selected());
        }
        break;
    default: break;
    }
    if (home_.Selected() != before) {
        audio.Play(Sfx::Move);
    }
}

void App::HandleStick(float dt) {
    if (settings_open || emulator.IsRunning() || std::abs(stick_x_) < kStickThreshold) {
        stick_held_ = false;
        return;
    }
    const Action dir = stick_x_ < 0 ? Action::Left : Action::Right;
    if (!stick_held_) {
        stick_held_ = true;
        stick_repeat_ = kStickFirstRepeat;
        HandleAction(dir);
        return;
    }
    stick_repeat_ -= dt;
    if (stick_repeat_ <= 0.0f) {
        stick_repeat_ = kStickRepeat;
        HandleAction(dir);
    }
}

void App::OpenSettings() {
    audio.Play(Sfx::Open);
    settings_open = true;
    settings_panel_.Open(*this);
}

// La musique du jeu ne démarre qu'après une courte pause sur la tuile, comme sur PS4,
// pour ne pas décoder un fichier à chaque déplacement rapide.
void App::UpdateMusic() {
    constexpr double kDelay = 0.8;
    const int selected = home_.Selected();
    if (selected != music_selection_) {
        music_selection_ = selected;
        selection_changed_at_ = Now();
        audio.PlayMusic({});
        return;
    }
    if (emulator.IsRunning()) {
        audio.PlayMusic({});
        return;
    }
    auto& games = library.Games();
    if (Now() - selection_changed_at_ >= kDelay && selected < static_cast<int>(games.size())) {
        audio.PlayMusic(games[selected].music_path);
    }
}

void App::LaunchGame(Game& game) {
    std::string error;
    if (settings.game_profiles && !settings.emulator_path.empty() &&
        !ApplyProfile(settings.emulator_path, game.title_id, error) && !error.empty()) {
        ShowToast(error);
        error.clear();
    }
    if (!emulator.Launch(settings.emulator_path, game.root, settings.fullscreen, error)) {
        ShowToast(error);
        if (settings.emulator_path.empty()) {
            OpenSettings();
        }
        return;
    }
    running_title = game.title;
    audio.PlayMusic({});
    if (settings.minimize_while_playing) {
        SDL_MinimizeWindow(window);
    }
}

void App::UpdateRunningGame() {
    const auto code = emulator.PollExit();
    if (!code) {
        return;
    }
    if (settings.minimize_while_playing) {
        SDL_RestoreWindow(window);
        SDL_RaiseWindow(window);
    }
    if (*code != 0) {
        ShowToast("shadPS4 s'est arrêté avec le code " + std::to_string(*code));
    }
    running_title.clear();
}

void App::Rescan() {
    library.Scan(renderer, settings.game_dirs);
}

void App::SaveSettings() {
    if (!settings.Save(settings_file_)) {
        ShowToast("Impossible d'enregistrer les réglages");
    }
}

void App::ShowToast(std::string message) {
    toast = std::move(message);
    toast_until = Now() + kToastSeconds;
}

void App::AddDroppedPath(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        ShowToast("Glissez un dossier de jeu (contenant eboot.bin), pas un fichier");
        return;
    }
    if (settings.AddGameDir(path)) {
        SaveSettings();
        const std::size_t before = library.Games().size();
        Rescan();
        const std::size_t added = library.Games().size() - std::min(before, library.Games().size());
        ShowToast(added > 0 ? std::to_string(added) + " jeu(x) ajouté(s)"
                            : "Aucun jeu trouvé dans ce dossier");
    } else {
        ShowToast("Ce dossier est déjà dans la bibliothèque");
    }
}

void App::RequestGameFolder() {
    SDL_ShowOpenFolderDialog(
        [](void* userdata, const char* const* files, int) {
            auto* app = static_cast<App*>(userdata);
            if (!files) {
                return;
            }
            std::lock_guard lock(app->dialog_mutex_);
            for (; *files; ++files) {
                app->pending_game_dirs_.push_back(PathFromUtf8(*files));
            }
        },
        this, window, nullptr, false);
}

void App::RequestEmulatorPath() {
    SDL_ShowOpenFileDialog(
        [](void* userdata, const char* const* files, int) {
            auto* app = static_cast<App*>(userdata);
            if (!files || !*files) {
                return;
            }
            std::lock_guard lock(app->dialog_mutex_);
            app->pending_emulator_ = PathFromUtf8(*files);
        },
        this, window, nullptr, 0, nullptr, false);
}

void App::ProcessDialogResults() {
    std::vector<fs::path> dirs;
    fs::path emulator_path;
    {
        std::lock_guard lock(dialog_mutex_);
        dirs.swap(pending_game_dirs_);
        emulator_path.swap(pending_emulator_);
    }
    for (const auto& dir : dirs) {
        AddDroppedPath(dir);
    }
    if (!emulator_path.empty()) {
        settings.emulator_path = emulator_path;
        SaveSettings();
    }
}

void App::DrawRunningOverlay(ImDrawList* dl) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float s = size.y / 1080.0f;
    dl->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 8, 25, 200));
    const std::string line = "En cours : " + running_title;
    const ImVec2 tsz = Theme::TextSize(fonts.title, s, line.c_str());
    Theme::DrawTextShadow(dl, fonts.title, s, ImVec2((size.x - tsz.x) / 2, size.y * 0.42f),
                          Theme::kText, line.c_str());
    const char* hint = "Forcer l'arrêt  (Échap)";
    const ImVec2 hsz = Theme::TextSize(fonts.body, s, hint);
    const ImVec2 hp((size.x - hsz.x) / 2 + 20 * s, size.y * 0.42f + tsz.y + 30 * s);
    Theme::DrawPadButton(dl, ImVec2(hp.x - 26 * s, hp.y + hsz.y / 2), 14 * s,
                         Theme::PadButton::Circle);
    Theme::DrawTextShadow(dl, fonts.body, s, hp, Theme::kTextDim, hint);
}

void App::DrawToast(ImDrawList* dl) {
    if (Now() >= toast_until || toast.empty()) {
        return;
    }
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    const float s = size.y / 1080.0f;
    const float remaining = static_cast<float>(toast_until - Now());
    const float alpha = std::clamp(remaining * 2.0f, 0.0f, 1.0f);
    const ImVec2 tsz = Theme::TextSize(fonts.body, s, toast.c_str());
    const ImVec2 p0(size.x - tsz.x - 120 * s, 110 * s);
    const ImVec2 p1(size.x - 80 * s, p0.y + tsz.y + 32 * s);
    dl->AddRectFilled(p0, p1, IM_COL32(10, 30, 70, static_cast<int>(230 * alpha)), 8 * s);
    dl->AddRectFilled(p0, ImVec2(p0.x + 5 * s, p1.y), IM_COL32(0, 112, 204, static_cast<int>(255 * alpha)),
                      8 * s, ImDrawFlags_RoundCornersLeft);
    Theme::DrawLabel(dl, fonts.body, s, ImVec2(p0.x + 22 * s, p0.y + 16 * s),
                     IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)), toast.c_str());
}
