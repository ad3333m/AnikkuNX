#pragma once

#include <borealis.hpp>
#include <nlohmann/json.hpp>

#include <climits>
#include <memory>
#include <vector>

#include "app/meta.hpp"
#include "util/async.hpp"
#include "view/cover_image.hpp"

class EpisodeDataSource;
class SeriesHero;
class EpisodeRow;

/** Series page in the Crunchyroll layout: artwork header, then a grid of episode cards. */
class AnimeActivity : public brls::Activity {
  public:
    AnimeActivity(std::string sourceId, std::string url, std::string title, std::string thumbnail);
    ~AnimeActivity() override;

    brls::View* createContentView() override;
    void onContentAvailable() override;
    void willAppear(bool resetState) override;

    void playEpisode(int index, const std::string& forcedToken = "", double startAt = -1);
    void chooseVideo(int index);
    /** Mette l'episodio nella coda dei download (offline). */
    void downloadEpisode(int index);
    void episodeOptions(int index);
    void openSeason(int index);
    void toggleOrder();
    void toggleLibrary();
    void playContinue();

    // read by the header and episode cells
    const nlohmann::json& items() const;
    bool seasons() const;
    int continueIndex() const;
    std::string sourceId, url, title, thumbnail;
    nlohmann::json data;
    std::shared_ptr<const meta::Info> info;
    bool inLibrary = false;
    bool loadedOnce = false;
    bool oldestFirst = false;  // ordine episodi: dal primo invece che dall'ultimo
    std::string error;
    SeriesHero* hero = nullptr;
    std::vector<EpisodeRow*> rows;

  private:
    void load(bool cached = false);
    void loadMeta();
    void render();
    void refreshCells();

    AliveToken alive = makeAlive();
    brls::RecyclerFrame* recycler = nullptr;
    EpisodeDataSource* dataSource = nullptr;
    size_t shownCount = SIZE_MAX;
};
