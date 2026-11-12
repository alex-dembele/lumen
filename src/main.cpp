#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "app.h"

// Options en ligne de commande :
//   --boot <TITLE_ID>        lance directement un jeu (utile pour un raccourci bureau)
//   --config-dir <dossier>   utilise un autre dossier de réglages
//   --screenshot <png>       rend l'interface hors écran et enregistre une capture
//   --select <n>             sélectionne la n-ième tuile au démarrage
//   --settings               ouvre directement les réglages
//   --info                   ouvre la fiche du jeu sélectionné
int main(int argc, char* argv[]) {
    LaunchOptions options;
    for (int i = 1; i < argc; ++i) {
        const bool has_value = i + 1 < argc;
        if (!std::strcmp(argv[i], "--config-dir") && has_value) {
            options.config_dir = argv[++i];
        } else if (!std::strcmp(argv[i], "--screenshot") && has_value) {
            options.screenshot = argv[++i];
        } else if (!std::strcmp(argv[i], "--select") && has_value) {
            options.select = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--boot") && has_value) {
            options.boot_title_id = argv[++i];
        } else if (!std::strcmp(argv[i], "--info")) {
            options.open_info = true;
        } else if (!std::strcmp(argv[i], "--settings")) {
            options.open_settings = true;
        }
    }

    App app;
    if (!app.Init(options)) {
        app.Shutdown();
        return 1;
    }
    const int code = app.Run();
    app.Shutdown();
    return code;
}
