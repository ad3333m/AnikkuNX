#pragma once

#include <borealis.hpp>
#include <nlohmann/json.hpp>

#include <functional>
#include <map>

#include "util/async.hpp"
#include "view/anime_grid.hpp"
#include "view/crunchy.hpp"

/** Controlla i nuovi episodi della libreria appena la console e' connessa a Internet. */
void checkNewEpisodesWhenOnline(int attempt = 0);

class MainActivity : public brls::Activity {
  public:
    brls::View* createContentView() override;
    void onContentAvailable() override;
};

/** Secondary screen opened from the home top bar: wraps a view in the standard applet frame. */
class ScreenActivity : public brls::Activity {
  public:
    ScreenActivity(std::string title, std::function<brls::View*()> make);
    brls::View* createContentView() override;

  private:
    std::string title;
    std::function<brls::View*()> make;
};

/** Base per le schede: gestisce il token di vita per le richieste asincrone. */
class TabBase : public brls::Box {
  public:
    TabBase();
    ~TabBase() override;

  protected:
    AliveToken alive = makeAlive();
};

/**
 * The single home screen, laid out like Crunchyroll: a fixed top bar, a full-bleed
 * hero carousel and a vertical stack of poster shelves (Continue Watching, one or
 * two shelves per enabled source, My List).
 */
class HomeView : public TabBase {
  public:
    HomeView();
    ~HomeView() override;
    void willAppear(bool resetState) override;
    brls::View* getDefaultFocus() override;
    void refreshLibrary();
    static HomeView* current;

  private:
    brls::Box* buildTopBar();
    brls::Box* buildHero();
    void rebuildSourceRows();
    void loadContinueWatching();
    void continueOptions(const GridItem& it);
    void offerSlides(int rank, const std::vector<GridItem>& items, const std::string& sourceName,
                     const std::string& lang);
    void showSlide(int index);
    void scheduleRotation();
    void loadSlideDetails(int index);
    void watchCurrent();
    void toggleMyList();
    static std::string sourcesSignature();

    brls::Box* content = nullptr;
    brls::Box* hero = nullptr;
    cr::HeroBackdrop* backdrop = nullptr;
    CoverImage* heroPoster = nullptr;
    brls::Label* heroKicker = nullptr;
    brls::Label* heroTitle = nullptr;
    brls::Label* heroMeta = nullptr;
    brls::Label* heroDesc = nullptr;
    brls::Box* watchBtn = nullptr;
    brls::Label* watchLabel = nullptr;
    brls::Box* listBtn = nullptr;
    cr::IconView* listIcon = nullptr;
    cr::PageDots* dots = nullptr;

    struct Slide {
        GridItem item;
        std::string sourceName, lang;
    };
    std::vector<Slide> slides;
    int slidesRank = 1 << 30;
    int slide = 0;
    int rotationGen = 0;
    int detailsGen = 0;
    std::map<std::string, nlohmann::json> details;  // url -> {description, genre}

    cr::PosterRow* continueRow = nullptr;
    cr::PosterRow* libraryRow = nullptr;
    brls::Box* primaryRows = nullptr;
    brls::Box* otherRows = nullptr;
    int sourcesGen = 0;
    std::string shownSources;
    bool appeared = false;
};

class HistoryTab : public TabBase {
  public:
    HistoryTab();
    void willAppear(bool resetState) override;

  private:
    void reload();
    AnimeGrid* grid;
    bool appeared = false;
};

class LibraryTab : public TabBase {
  public:
    LibraryTab();
    ~LibraryTab() override;
    void willAppear(bool resetState) override;
    void reload();
    /** Scheda Libreria attualmente creata (per aggiornarla dopo il controllo dei nuovi episodi). */
    static LibraryTab* current;

  private:
    AnimeGrid* grid;
    bool appeared = false;
};

class SourcesTab : public TabBase {
  public:
    SourcesTab();
    void willAppear(bool resetState) override;

  private:
    void reload();
    brls::Box* list;
    bool appeared = false;
};

class SearchTab : public TabBase {
  public:
    explicit SearchTab(bool askNow = false);

  private:
    void ask();
    void search(const std::string& q);
    AnimeGrid* grid;
    brls::Button* button;
    std::string query;
    int generation = 0;
    int pending = 0;
};

class SettingsTab : public TabBase {
  public:
    SettingsTab();
};
