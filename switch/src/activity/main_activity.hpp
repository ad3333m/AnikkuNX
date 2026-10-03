#pragma once

#include <borealis.hpp>
#include <nlohmann/json.hpp>

#include "util/async.hpp"
#include "view/anime_grid.hpp"
#include "view/anime_row.hpp"

/** Controlla i nuovi episodi della libreria appena la console e' connessa a Internet. */
void checkNewEpisodesWhenOnline(int attempt = 0);

class MainActivity : public brls::Activity {
  public:
    brls::View* createContentView() override;
    void onContentAvailable() override;
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
 * Netflix/Crunchyroll-style landing tab: a hero banner on top, then a
 * stack of horizontal rows (Continue watching, Trending, Library, one row
 * per enabled source). Loads popular items from every enabled source so
 * the user picks straight from thumbnails instead of a sources list.
 */
class DiscoverTab : public TabBase {
  public:
    DiscoverTab();
    void willAppear(bool resetState) override;

  private:
    void buildStatic();
    void reloadDynamic();
    void loadContinueWatching();
    void loadLibraryRow();
    void loadSourceRow(const std::string& sourceId, const std::string& sourceName, AnimeRow* row);
    void setHero(const GridItem& item, const std::string& sourceName);

    brls::Box* content = nullptr;
    brls::Box* heroBox = nullptr;
    brls::Label* heroTitle = nullptr;
    brls::Label* heroMeta = nullptr;
    brls::Label* heroHint = nullptr;
    brls::Box* heroHintBox = nullptr;
    GridItem heroItem;
    bool heroReady = false;
    AnimeRow* continueRow = nullptr;
    AnimeRow* libraryRow = nullptr;
    std::vector<AnimeRow*> sourceRows;
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
    SearchTab();

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
