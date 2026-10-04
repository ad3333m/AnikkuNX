#ifdef IOS
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_main.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#endif

#include <borealis.hpp>

#include <cstdlib>
#include <cstring>

#include "activity/main_activity.hpp"
#include "app/api.hpp"
#include "app/downloads.hpp"
#include "config.hpp"
#include "net/http.hpp"
#include "util/i18n.hpp"
#include "app/updater.hpp"
#include "util/async.hpp"
#include "view/cover_image.hpp"

#ifdef IOS
/** Resources are copied into <App>.app/assets (a root folder named "Resources" in any case makes CFBundle
 *  treat the app as an old-style bundle and miss Info.plist); borealis and the app open them relative to the cwd. */
static void enterBundleResources() {
    CFURLRef url = CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle());
    char path[4096];
    if (url && CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, sizeof(path)))
        chdir((std::string(path) + "/assets").c_str());
    if (url) CFRelease(url);
}
#endif

int main(int argc, char* argv[]) {
#ifdef IOS
    enterBundleResources();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
#endif
    if (argc > 0 && argv[0]) updater::setAppPath(argv[0]);  // per sostituire il .nro negli aggiornamenti
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-d") == 0) brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
    }

    brls::Platform::APP_LOCALE_DEFAULT = "en-US";  // English-only, also for borealis' own hints
    if (!brls::Application::init()) {
        brls::Logger::error("Impossibile inizializzare borealis");
        return EXIT_FAILURE;
    }
    i18n::init();


#ifdef __SWITCH__
    http::globalInit("romfs:/cacert.pem");
#else
    http::globalInit(std::string(BRLS_RESOURCES) + "cacert.pem");
#endif
    Config::instance().load();
    Config::instance().checkPreviousCrash();
    Config::instance().save();  // crea la cartella dati se manca
    Config::instance().applyDomains();
    api::init(Config::instance().configDir());
    downloads::init(Config::instance().configDir());  // coda dei download offline (thread dedicato)

    brls::Application::createWindow("AnikkuNX");
#ifdef __SWITCH__
    // (dopo createWindow: prima non esistono ne' il contesto grafico ne' i font di sistema)
    // Font predefinito = font standard della console (vedi scripts/setup.sh): i caratteri che non ha
    // (cinese, coreano, simboli dei pulsanti, icone) vengono presi dagli altri font di sistema.
    {
        NVGcontext* vg = brls::Application::getNVGContext();
        int regular = brls::Application::getFont(brls::FONT_REGULAR);
        for (const std::string& name :
             {brls::FONT_CHINESE_SIMPLIFIED, brls::FONT_CHINESE_SIMPLIFIED_EXT, brls::FONT_CHINESE_TRADITIONAL,
              brls::FONT_KOREAN_REGULAR, brls::FONT_SWITCH_ICONS, brls::FONT_MATERIAL_ICONS}) {
            int f = brls::Application::getFont(name);
            if (regular >= 0 && f >= 0) nvgAddFallbackFontId(vg, regular, f);
        }
        // hindi (devanagari), ebraico...: i font della console non li hanno.
        // GNU FreeSans (GPL con eccezione per i font) li copre; senza shaping le legature indiane sono semplificate.
        if (regular >= 0 && brls::Application::loadFontFromFile("freesans", "romfs:/font/FreeSans.ttf"))
            nvgAddFallbackFontId(vg, regular, brls::Application::getFont("freesans"));
        // arabo: FreeSans copre pochissimo l'arabo (quasi solo punteggiatura), da qui i quadratini vuoti.
        // Noto Naskh Arabic UI (SIL OFL) ha le lettere; senza shaping restano nella forma isolata (non collegate).
        if (regular >= 0 && brls::Application::loadFontFromFile("notoarabic", "romfs:/font/NotoNaskhArabic.ttf"))
            nvgAddFallbackFontId(vg, regular, brls::Application::getFont("notoarabic"));
    }
#endif
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
    brls::Application::setGlobalQuit(false);

    // Theme: Crunchyroll-style black background with orange accents
    auto& theme = brls::Theme::getDarkTheme();
    theme.addColor("brls/clear", nvgRGB(0, 0, 0));
    theme.addColor("brls/background", nvgRGB(0, 0, 0));
    theme.addColor("brls/highlight/background", nvgRGB(28, 29, 34));
    theme.addColor("brls/click_pulse", nvgRGBA(244, 117, 33, 38));
    theme.addColor("brls/sidebar/background", nvgRGB(20, 21, 25));
    theme.addColor("brls/sidebar/separator", nvgRGB(42, 43, 50));
    theme.addColor("brls/header/border", nvgRGB(42, 43, 50));
    theme.addColor("brls/header/rectangle", nvgRGB(244, 117, 33));
    theme.addColor("brls/applet_frame/separator", nvgRGB(42, 43, 50));
    theme.addColor("brls/list/listItem_value_color", nvgRGB(244, 117, 33));
    theme.addColor("brls/button/default_enabled_background", nvgRGB(35, 37, 43));
    theme.addColor("brls/button/enabled_border_color", nvgRGB(244, 117, 33));
    theme.addColor("brls/button/highlight_enabled_text", nvgRGB(244, 117, 33));
    theme.addColor("brls/accent", nvgRGB(244, 117, 33));
    theme.addColor("brls/highlight/color1", nvgRGB(244, 117, 33));
    theme.addColor("brls/highlight/color2", nvgRGB(255, 190, 130));
    theme.addColor("brls/button/primary_enabled_background", nvgRGB(244, 117, 33));
    theme.addColor("brls/button/primary_enabled_text", nvgRGB(0, 0, 0));
    theme.addColor("brls/sidebar/active_item", nvgRGB(244, 117, 33));
    theme.addColor("brls/slider/line_filled", nvgRGB(244, 117, 33));

    // Barra laterale piu' stretta: lascia spazio alla griglia delle copertine
    brls::getStyle().addMetric("brls/tab_frame/sidebar_width", 300);
    brls::getStyle().addMetric("brls/sidebar/padding_left", 30);
    brls::getStyle().addMetric("brls/sidebar/padding_right", 20);

    brls::Application::pushActivity(new MainActivity());

    while (brls::Application::mainLoop())
        ;

    ThreadPool::instance().stop();
    CoverImage::clearCache();
    http::globalCleanup();
    return EXIT_SUCCESS;
}
