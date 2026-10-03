#include "activity/main_activity.hpp"
#include "util/i18n.hpp"

#include <algorithm>
#include <cctype>

#include "activity/anime_activity.hpp"
#include "activity/browse_activity.hpp"
#include "activity/downloads_activity.hpp"
#include "activity/source_picker.hpp"
#include "activity/update_activity.hpp"
#include "util/network.hpp"
#include "util/platform.hpp"

#ifndef UPDATE_REPO
#define UPDATE_REPO "ad3333m/AnikkuNX"
#endif
#define UPDATE_REPO_DISPLAY UPDATE_REPO
#include "config.hpp"
#include "util/sublang.hpp"
#include "app/api.hpp"
#include "sources/source.hpp"

using json = nlohmann::json;

static std::string fmtTime(double s) {
    long t = (long)s;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02ld:%02ld", t / 60, t % 60);
    return buf;
}

static brls::ScrollingFrame* scrollOf(brls::Box* content) {
    auto* sf = new brls::ScrollingFrame();
    sf->setGrow(1);
    sf->setContentView(content);
    return sf;
}

static brls::Label* header(const std::string& text) {
    auto* l = new brls::Label();
    l->setText(text);
    l->setFontSize(15);
    l->setTextColor(nvgRGB(150, 150, 160));
    l->setMargins(4, 0, 10, 0);
    return l;
}

// ============================================================================ MainActivity

namespace {

void showFullMemoryHelp() {
    auto* d = new brls::Dialog(tr(
        "AnikkuNX e' stata avviata in modalita' applet (dall'Album): la memoria disponibile e' molto ridotta e "
        "i video possono bloccarsi o chiudere l'app.\n\n"
        "Per avere la memoria piena:\n"
        "\xE2\x80\xA2 tieni premuto R mentre avvii un gioco qualsiasi, poi apri AnikkuNX dal menu homebrew;\n"
        "\xE2\x80\xA2 oppure crea un forwarder con Sphaira (X su AnikkuNX > Install Forwarder) e avviala dalla Home."));
    d->addButton(tr("OK"), [] {});
    d->open();
}

/** Orange crescent mark in front of the wordmark. */
class LogoMark : public brls::View {
  public:
    LogoMark() {
        setWidth(34);
        setHeight(34);
    }
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) override {
        float cx = x + w / 2, cy = y + h / 2;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, 15);
        nvgFillColor(vg, cr::orange());
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, cx + 4.5f, cy - 2.5f, 8.5f);
        nvgFillColor(vg, cr::bar());
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircle(vg, cx + 6.5f, cy - 4.5f, 3.5f);
        nvgFillColor(vg, cr::orange());
        nvgFill(vg);
    }
};

/** Solid play triangle for the "Start watching" button. */
class PlayGlyph : public brls::View {
  public:
    explicit PlayGlyph(NVGcolor c) : color(c) {
        setWidth(15);
        setHeight(18);
    }
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) override {
        nvgBeginPath(vg);
        nvgMoveTo(vg, x + 1, y + 1);
        nvgLineTo(vg, x + w - 1, y + h / 2);
        nvgLineTo(vg, x + 1, y + h - 1);
        nvgClosePath(vg);
        nvgFillColor(vg, color);
        nvgFill(vg);
    }

  private:
    NVGcolor color;
};

std::string upper(std::string s) {
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

std::string oneLine(std::string s) {
    for (auto& c : s)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    std::string out;
    for (char c : s)
        if (!(c == ' ' && !out.empty() && out.back() == ' ')) out += c;
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out;
}

void openAnime(const GridItem& it) {
    brls::Application::pushActivity(new AnimeActivity(it.sourceId, it.url, it.title, it.thumbnail));
}

void openScreen(const std::string& title, std::function<brls::View*()> make) {
    brls::Application::pushActivity(new ScreenActivity(title, std::move(make)));
}

}  // namespace

brls::View* MainActivity::createContentView() { return new HomeView(); }

void MainActivity::onContentAvailable() {
    // il player si era chiuso in modo anomalo: spiega cosa e' stato disattivato
    std::string notice = Config::instance().crashNotice;
    Config::instance().crashNotice.clear();
    if (!notice.empty()) {
        std::string text = tr("L'app si e' chiusa durante la riproduzione. Ho disattivato il proxy per gli stream "
                              "camuffati (Impostazioni > Riproduzione): se il problema sparisce, era quella la causa.");
        brls::delay(800, [text] {
            auto* d = new brls::Dialog(text);
            d->addButton(tr("OK"), [] {});
            d->open();
        });
    }
    // primo avvio: scelta delle fonti da attivare
    if (!Config::instance().sourcesChosen)
        brls::delay(100, [] { brls::Application::pushActivity(new SourcePickerActivity(true)); });
    else {
        if (Config::instance().checkUpdates)
            brls::delay(1500, [] { checkForUpdates(false); });  // nuova versione su GitHub?
        if (Config::instance().checkNewEpisodes)
            brls::delay(4000, [] { checkNewEpisodesWhenOnline(); });  // nuovi episodi in libreria
    }
}

ScreenActivity::ScreenActivity(std::string t, std::function<brls::View*()> m) : title(std::move(t)), make(std::move(m)) {}

brls::View* ScreenActivity::createContentView() {
    brls::View* v = make();
    v->getAppletFrameItem()->title = title;
    v->getAppletFrameItem()->iconPath = BRLS_ASSET("icon/icon_96.png");
    return new brls::AppletFrame(v);
}

// ============================================================================ TabBase

TabBase::TabBase() : brls::Box(brls::Axis::COLUMN) { this->setGrow(1); }

TabBase::~TabBase() { *alive = false; }

static std::vector<GridItem> gridFromAnimeArray(const json& arr) {
    std::vector<GridItem> items;
    for (auto& a : arr) {
        GridItem it{a.value("sourceId", ""), a.value("url", ""), a.value("title", ""), a.value("sourceName", ""),
                    a.value("thumbnail", ""), a};
        int n = a.value("newEpisodes", 0);
        if (n > 0) {
            it.badge = "+" + std::to_string(n);
            it.subtitle = n == 1 ? tr("1 episodio nuovo") : tr("{} episodi nuovi", std::to_string(n));
        }
        items.push_back(it);
    }
    return items;
}

// ============================================================================ Home

HomeView* HomeView::current = nullptr;

HomeView::HomeView() {
    current = this;
    setBackgroundColor(nvgRGB(0, 0, 0));
    addView(buildTopBar());

    content = new brls::Box(brls::Axis::COLUMN);
    content->setAlignItems(brls::AlignItems::STRETCH);
    content->setPaddingBottom(20);
    content->addView(buildHero());

    continueRow = new cr::PosterRow("Continue Watching");
    continueRow->onSelect = openAnime;
    continueRow->progressOf = [](const GridItem& it) -> float {
        if (it.extra.value("watched", false)) return 1.0f;
        double d = it.extra.value("duration", 0.0), p = it.extra.value("position", 0.0);
        return d > 0 ? (float)(p / d) : -1.0f;
    };
    continueRow->onSeeAll = [] { openScreen("History", [] { return new HistoryTab(); }); };
    continueRow->setVisibility(brls::Visibility::GONE);
    content->addView(continueRow);

    primaryRows = new brls::Box(brls::Axis::COLUMN);
    primaryRows->setAlignItems(brls::AlignItems::STRETCH);
    content->addView(primaryRows);

    libraryRow = new cr::PosterRow("My List");
    libraryRow->onSelect = openAnime;
    libraryRow->onSeeAll = [] { openScreen("My List", [] { return new LibraryTab(); }); };
    libraryRow->setVisibility(brls::Visibility::GONE);
    content->addView(libraryRow);

    otherRows = new brls::Box(brls::Axis::COLUMN);
    otherRows->setAlignItems(brls::AlignItems::STRETCH);
    content->addView(otherRows);

    auto* foot = cr::label("AnikkuNX v" + updater::currentVersion() + "    \xC2\xB7    Press + to exit", 14, cr::dim());
    foot->setMargins(16, 60, 10, 60);
    content->addView(foot);

    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1);
    scroll->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
    scroll->setScrollingIndicatorVisible(false);
    scroll->setContentView(content);
    addView(scroll);

    registerAction("Exit", brls::BUTTON_START, [](brls::View*) {
        brls::Application::quit();
        return true;
    });

    loadContinueWatching();
    refreshLibrary();
    rebuildSourceRows();
}

HomeView::~HomeView() {
    if (current == this) current = nullptr;
}

brls::View* HomeView::getDefaultFocus() { return watchBtn; }

void HomeView::willAppear(bool resetState) {
    TabBase::willAppear(resetState);
    if (!appeared) {
        appeared = true;
        return;
    }
    loadContinueWatching();
    refreshLibrary();
    if (sourcesSignature() != shownSources) rebuildSourceRows();  // changed in Settings > Choose sources
}

brls::Box* HomeView::buildTopBar() {
    auto* bar = new brls::Box(brls::Axis::ROW);
    bar->setHeight(64);
    bar->setBackgroundColor(cr::bar());
    bar->setAlignItems(brls::AlignItems::CENTER);
    bar->setPadding(0, 24, 0, 36);

    auto* logo = new brls::Box(brls::Axis::ROW);
    logo->setAlignItems(brls::AlignItems::CENTER);
    logo->setMarginRight(26);
    logo->addView(new LogoMark());
    auto* word = cr::label("AnikkuNX", 24, cr::orange(), true);
    word->setMarginLeft(8);
    logo->addView(word);
    bar->addView(logo);

    bar->addView(new cr::NavItem("Browse", [] { openScreen("Browse", [] { return new SourcesTab(); }); }));
    bar->addView(new cr::NavItem("My List", [] { openScreen("My List", [] { return new LibraryTab(); }); }));
    bar->addView(new cr::NavItem("History", [] { openScreen("History", [] { return new HistoryTab(); }); }));
    bar->addView(new cr::NavItem("Downloads", [] { openScreen("Downloads", [] { return new DownloadsTab(); }); }));

    auto* spacer = new brls::Box(brls::Axis::ROW);
    spacer->setGrow(1);
    bar->addView(spacer);

    if (platform::isAppletMode()) bar->addView(new cr::NavItem("Limited memory", [] { showFullMemoryHelp(); }));
    bar->addView(new cr::NavIcon(cr::ICON_SEARCH, [] { openScreen("Search", [] { return new SearchTab(true); }); }));
    bar->addView(new cr::NavIcon(cr::ICON_SETTINGS, [] { openScreen("Settings", [] { return new SettingsTab(); }); }));
    return bar;
}

brls::Box* HomeView::buildHero() {
    hero = new brls::Box(brls::Axis::ROW);
    hero->setHeight(470);
    hero->setAlignItems(brls::AlignItems::CENTER);
    hero->setMarginBottom(4);

    backdrop = new cr::HeroBackdrop();
    hero->addView(backdrop);

    auto* col = new brls::Box(brls::Axis::COLUMN);
    col->setGrow(1);
    col->setPadding(0, 40, 0, 80);
    col->setJustifyContent(brls::JustifyContent::CENTER);
    col->setAlignItems(brls::AlignItems::FLEX_START);

    heroKicker = cr::label(" ", 14, cr::orange(), true);
    col->addView(heroKicker);

    heroTitle = cr::label("Loading\xE2\x80\xA6", 40, cr::text(), true);
    heroTitle->setWidth(660);
    heroTitle->setMarginTop(8);
    col->addView(heroTitle);

    heroMeta = cr::label(" ", 15, cr::muted());
    heroMeta->setMarginTop(10);
    col->addView(heroMeta);

    auto* descBox = new brls::Box(brls::Axis::COLUMN);
    descBox->setWidth(600);
    descBox->setHeight(104);
    descBox->setMarginTop(10);
    heroDesc = cr::label("", 16, nvgRGB(225, 225, 230));
    heroDesc->setWidth(600);
    descBox->addView(heroDesc);
    col->addView(descBox);

    auto* buttons = new brls::Box(brls::Axis::ROW);
    buttons->setAlignItems(brls::AlignItems::CENTER);
    buttons->setMarginTop(12);

    watchBtn = new brls::Box(brls::Axis::ROW);
    watchBtn->setFocusable(true);
    watchBtn->setHeight(46);
    watchBtn->setPadding(0, 22, 0, 18);
    watchBtn->setAlignItems(brls::AlignItems::CENTER);
    watchBtn->setBackgroundColor(cr::orange());
    watchBtn->setHideHighlightBackground(true);
    watchBtn->setHighlightCornerRadius(2);
    watchBtn->setHighlightPadding(4);
    watchBtn->addView(new PlayGlyph(nvgRGB(0, 0, 0)));
    watchLabel = cr::label("START WATCHING", 16, nvgRGB(0, 0, 0), true);
    watchLabel->setMarginLeft(10);
    watchBtn->addView(watchLabel);
    watchBtn->registerClickAction([this](brls::View*) {
        watchCurrent();
        return true;
    });
    watchBtn->addGestureRecognizer(new brls::TapGestureRecognizer(watchBtn));
    buttons->addView(watchBtn);

    listBtn = new brls::Box(brls::Axis::ROW);
    listBtn->setFocusable(true);
    listBtn->setWidth(46);
    listBtn->setHeight(46);
    listBtn->setMarginLeft(14);
    listBtn->setJustifyContent(brls::JustifyContent::CENTER);
    listBtn->setAlignItems(brls::AlignItems::CENTER);
    listBtn->setBorderColor(cr::orange());
    listBtn->setBorderThickness(2);
    listBtn->setHideHighlightBackground(true);
    listBtn->setHighlightCornerRadius(2);
    listBtn->setHighlightPadding(4);
    listIcon = new cr::IconView(cr::ICON_BOOKMARK_BORDER, 26, cr::orange());
    listBtn->addView(listIcon);
    listBtn->registerClickAction([this](brls::View*) {
        toggleMyList();
        return true;
    });
    listBtn->addGestureRecognizer(new brls::TapGestureRecognizer(listBtn));
    listBtn->setVisibility(brls::Visibility::GONE);
    buttons->addView(listBtn);
    col->addView(buttons);

    dots = new cr::PageDots();
    dots->setMarginTop(32);
    col->addView(dots);
    hero->addView(col);

    heroPoster = new CoverImage();
    heroPoster->setWidth(236);
    heroPoster->setHeight(354);
    heroPoster->setMarginRight(110);
    heroPoster->setCornerRadius(2);
    heroPoster->setShadowType(brls::ShadowType::GENERIC);
    heroPoster->setVisibility(brls::Visibility::INVISIBLE);
    hero->addView(heroPoster);

    auto prev = [this] {
        if (!slides.empty()) showSlide((slide + (int)slides.size() - 1) % (int)slides.size());
    };
    auto next = [this] {
        if (!slides.empty()) showSlide((slide + 1) % (int)slides.size());
    };
    hero->addView(new cr::HeroArrow(false, prev));
    hero->addView(new cr::HeroArrow(true, next));
    hero->registerAction("Previous", brls::BUTTON_LB, [prev](brls::View*) {
        prev();
        return true;
    });
    hero->registerAction("Next", brls::BUTTON_RB, [next](brls::View*) {
        next();
        return true;
    });
    return hero;
}

std::string HomeView::sourcesSignature() {
    std::string s;
    for (auto& x : api::sources()) s += x.value("id", "") + ",";
    return s;
}

void HomeView::rebuildSourceRows() {
    int gen = ++sourcesGen;
    if (primaryRows->isChildFocused() || otherRows->isChildFocused()) brls::Application::giveFocus(watchBtn);
    primaryRows->clearViews();
    otherRows->clearViews();
    primaryRows->setLastFocusedView(nullptr);
    otherRows->setLastFocusedView(nullptr);

    slides.clear();
    slidesRank = 1 << 30;
    slide = 0;
    ++rotationGen;
    dots->setState(0, 0);
    listBtn->setVisibility(brls::Visibility::GONE);
    heroPoster->setVisibility(brls::Visibility::INVISIBLE);
    backdrop->clear();
    heroMeta->setText(" ");

    json arr = api::sources();
    shownSources = sourcesSignature();
    if (arr.empty()) {
        heroKicker->setText("GET STARTED");
        heroTitle->setText("Choose your anime sources");
        heroDesc->setText(cr::wrap("Pick the sites AnikkuNX streams from. You can change them any time with the gear "
                                   "icon in the top right.",
                                   16, 596));
        watchLabel->setText("CHOOSE SOURCES");
        return;
    }
    heroKicker->setText(" ");
    heroTitle->setText("Loading\xE2\x80\xA6");
    heroDesc->setText("");
    watchLabel->setText("START WATCHING");

    // English sources first (the app is English-only), then multi-language, then the rest
    std::vector<json> list(arr.begin(), arr.end());
    auto langRank = [](const json& s) {
        std::string l = s.value("lang", "");
        return l == "en" ? 0 : l == "all" ? 1 : 2;
    };
    std::stable_sort(list.begin(), list.end(), [&](const json& a, const json& b) { return langRank(a) < langRank(b); });

    auto addRow = [this, gen](brls::Box* parent, const std::string& title, const std::string& subtitle, const json& s,
                              const std::string& mode, int heroRank) {
        std::string id = s.value("id", ""), name = s.value("name", ""), lang = s.value("lang", "");
        bool latest = s.value("supportsLatest", false);
        auto* row = new cr::PosterRow(title, subtitle);
        row->onSelect = openAnime;
        row->onSeeAll = [id, name, latest] { brls::Application::pushActivity(new BrowseActivity(id, name, latest)); };
        row->setVisibility(brls::Visibility::GONE);  // appears once it has posters
        parent->addView(row);
        runAsync<json>(
            alive, [id, mode] { return api::browse(id, mode, 1); },
            [this, gen, row, name, lang, heroRank](json r) {
                if (gen != sourcesGen) return;
                std::vector<GridItem> items;
                for (auto& a : r.value("animes", json::array())) {
                    if (items.size() >= 18) break;
                    items.push_back({a.value("sourceId", ""), a.value("url", ""), a.value("title", ""), name,
                                     a.value("thumbnail", ""), a});
                }
                if (items.empty()) return;
                row->setItems(items);
                row->setVisibility(brls::Visibility::VISIBLE);
                if (heroRank >= 0) offerSlides(heroRank, items, name, lang);
            },
            [](const std::string&) {});
    };

    for (size_t i = 0; i < list.size(); i++) {
        const json& s = list[i];
        std::string name = s.value("name", "");
        if (i == 0) {
            addRow(primaryRows, "Trending on " + name, "", s, "popular", (int)i);
            if (s.value("supportsLatest", false))
                addRow(primaryRows, "New Episodes", "Just released on " + name, s, "latest", -1);
        } else {
            addRow(otherRows, "Popular on " + name, i18n::languageName(s.value("lang", "")), s, "popular", (int)i);
        }
    }
}

void HomeView::loadContinueWatching() {
    runAsync<json>(
        alive, [] { return api::history(); },
        [this](json arr) {
            std::vector<GridItem> items;
            for (auto& h : arr) {
                if (items.size() >= 15) break;
                std::string sub = h.value("watched", false) ? "Watched" : h.value("episodeName", "");
                items.push_back({h.value("sourceId", ""), h.value("animeUrl", ""), h.value("title", ""), sub,
                                 h.value("thumbnail", ""), h});
            }
            if (!items.empty()) continueRow->setItems(items);
            continueRow->setVisibility(items.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
        },
        [](const std::string&) {});
}

void HomeView::refreshLibrary() {
    runAsync<json>(
        alive, [] { return api::library(); },
        [this](json arr) {
            auto items = gridFromAnimeArray(arr);
            if (items.size() > 20) items.resize(20);
            if (!items.empty()) libraryRow->setItems(items);
            libraryRow->setVisibility(items.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
            // keep the hero bookmark in sync with changes made elsewhere
            for (auto& s : slides) {
                bool in = false;
                for (auto& a : arr)
                    if (a.value("sourceId", "") == s.item.sourceId && a.value("url", "") == s.item.url) in = true;
                s.item.extra["inLibrary"] = in;
            }
            if (!slides.empty())
                listIcon->setIcon(slides[slide].item.extra.value("inLibrary", false) ? cr::ICON_BOOKMARK
                                                                                     : cr::ICON_BOOKMARK_BORDER);
        },
        [](const std::string&) {});
}

void HomeView::offerSlides(int rank, const std::vector<GridItem>& items, const std::string& sourceName,
                           const std::string& lang) {
    if (rank >= slidesRank) return;  // a higher-priority source already fills the hero
    std::vector<Slide> next;
    for (auto& it : items) {
        if (it.thumbnail.empty()) continue;
        next.push_back({it, sourceName, lang});
        if (next.size() >= 6) break;
    }
    if (next.empty()) return;
    slidesRank = rank;
    slides = next;
    showSlide(0);
}

void HomeView::showSlide(int index) {
    if (slides.empty()) return;
    slide = index;
    const Slide& s = slides[index];
    backdrop->setUrl(s.item.thumbnail);
    heroPoster->setUrl(s.item.thumbnail);
    heroPoster->setVisibility(brls::Visibility::VISIBLE);
    heroKicker->setText(upper("Featured on " + s.sourceName));
    heroTitle->setText(cr::ellipsize(s.item.title, 64));
    watchLabel->setText("START WATCHING");
    listBtn->setVisibility(brls::Visibility::VISIBLE);
    listIcon->setIcon(s.item.extra.value("inLibrary", false) ? cr::ICON_BOOKMARK : cr::ICON_BOOKMARK_BORDER);
    dots->setState((int)slides.size(), index);
    loadSlideDetails(index);
    scheduleRotation();
}

void HomeView::loadSlideDetails(int index) {
    auto apply = [this](const Slide& s) {
        std::string meta = i18n::languageName(s.lang);
        std::string desc;
        auto it = details.find(s.item.url);
        if (it != details.end()) {
            std::string g = oneLine(it->second.value("genre", ""));
            if (!g.empty()) meta += "  \xC2\xB7  " + cr::ellipsize(g, 70);
            desc = oneLine(it->second.value("description", ""));
        }
        heroMeta->setText(meta);
        heroDesc->setText(cr::wrap(cr::ellipsize(desc, 230), 16, 596));
    };
    const Slide& s = slides[index];
    apply(s);
    // opening the details page marks library episodes as seen, so skip titles already in My List
    if (details.count(s.item.url) || s.item.extra.value("inLibrary", false)) return;
    std::string sid = s.item.sourceId, url = s.item.url, title = s.item.title;
    details[url] = json::object();  // one request per title
    runAsync<json>(
        alive,
        [sid, url, title] {
            json d = api::anime(sid, url, title, true);
            return json{{"description", d.value("description", "")}, {"genre", d.value("genre", "")}};
        },
        [this, url, apply](json d) {
            details[url] = d;
            if (!slides.empty() && slides[slide].item.url == url) apply(slides[slide]);
        },
        [](const std::string&) {});
}

void HomeView::scheduleRotation() {
    int gen = ++rotationGen;
    std::weak_ptr<bool> weak = alive;
    brls::delay(9000, [this, weak, gen] {
        auto a = weak.lock();
        if (!a || !*a || gen != rotationGen || slides.size() < 2) return;
        showSlide((slide + 1) % (int)slides.size());
    });
}

void HomeView::watchCurrent() {
    if (!slides.empty()) {
        openAnime(slides[slide].item);
        return;
    }
    if (shownSources.empty()) brls::Application::pushActivity(new SourcePickerActivity(false));
}

void HomeView::toggleMyList() {
    if (slides.empty()) return;
    GridItem& it = slides[slide].item;
    bool in = it.extra.value("inLibrary", false);
    it.extra["inLibrary"] = !in;
    listIcon->setIcon(in ? cr::ICON_BOOKMARK_BORDER : cr::ICON_BOOKMARK);
    std::string sid = it.sourceId, url = it.url, title = it.title;
    runAsync<bool>(
        alive,
        [sid, url, title, in] {
            if (in)
                api::removeLibrary(sid, url);
            else
                api::addLibrary(sid, url, title);
            return true;
        },
        [this, in](bool) {
            brls::Application::notify(in ? "Removed from My List" : "Added to My List");
            refreshLibrary();
        });
}

// ============================================================================ Nuovi episodi

void checkNewEpisodesWhenOnline(int attempt) {
    if (!network::connected()) {
        if (attempt < 120) brls::delay(5000, [attempt] { checkNewEpisodesWhenOnline(attempt + 1); });
        return;
    }
    static AliveToken appAlive = makeAlive();
    runAsync<json>(
        appAlive, [] { return api::refreshLibrary(); },
        [](json found) {
            if (LibraryTab::current) LibraryTab::current->reload();
            if (HomeView::current) HomeView::current->refreshLibrary();
            if (found.empty()) return;
            if (found.size() == 1)
                brls::Application::notify(tr("Nuovi episodi di {}", found[0].value("title", "")));
            else
                brls::Application::notify(tr("{} anime della libreria hanno nuovi episodi", std::to_string(found.size())));
        },
        [](const std::string&) {});
}

// ============================================================================ Cronologia

HistoryTab::HistoryTab() {
    grid = new AnimeGrid(5, 206);
    grid->onSelect = [](const GridItem& it) {
        brls::Application::pushActivity(new AnimeActivity(it.sourceId, it.url, it.title, it.thumbnail));
    };
    grid->secondaryHint = tr("Rimuovi");
    grid->onSecondary = [this](const GridItem& it) {
        auto* d = new brls::Dialog(tr("Togliere \"{}\" da Continua a guardare?", it.title));
        d->addButton(tr("Annulla"), [] {});
        d->addButton(tr("Rimuovi"), [this, it] {
            runAsync<bool>(
                alive,
                [it] {
                    api::removeFromHistory(it.sourceId, it.url);
                    return true;
                },
                [this](bool) { reload(); });
        });
        d->open();
    };
    this->addView(grid);
    reload();
}

void HistoryTab::willAppear(bool resetState) {
    TabBase::willAppear(resetState);
    if (appeared) reload();
    appeared = true;
}

void HistoryTab::reload() {
    grid->showLoading();
    runAsync<json>(
        alive, [] { return api::history(); },
        [this](json arr) {
            grid->focusFallback = this->getParent();
            bool refocus = grid->clear();
            std::vector<GridItem> items;
            for (auto& h : arr) {
                std::string sub = h.value("episodeName", "");
                if (h.value("watched", false))
                    sub += " \xC2\xB7 " + tr("visto");
                else if (h.value("duration", 0.0) > 0)
                    sub += " \xC2\xB7 " + fmtTime(h.value("position", 0.0));
                items.push_back({h.value("sourceId", ""), h.value("animeUrl", ""), h.value("title", ""), sub,
                                 h.value("thumbnail", ""), h});
            }
            grid->hideStatus();
            grid->append(items);
            if (items.empty()) grid->showMessage(tr("Non hai ancora guardato nulla.\nApri \"Sorgenti\" per iniziare!"));
            if (refocus && !items.empty()) brls::Application::giveFocus(grid);
        },
        [this](const std::string& err) { grid->showMessage(err); });
}

// ============================================================================ Libreria

LibraryTab* LibraryTab::current = nullptr;

LibraryTab::~LibraryTab() {
    if (current == this) current = nullptr;
}

LibraryTab::LibraryTab() {
    current = this;
    grid = new AnimeGrid(5, 206);
    grid->onSelect = [](const GridItem& it) {
        brls::Application::pushActivity(new AnimeActivity(it.sourceId, it.url, it.title, it.thumbnail));
    };
    grid->secondaryHint = tr("Rimuovi");
    grid->onSecondary = [this](const GridItem& it) {
        auto* d = new brls::Dialog(tr("Rimuovere \"{}\" dalla libreria?", it.title));
        d->addButton(tr("Annulla"), [] {});
        d->addButton(tr("Rimuovi"), [this, it] {
            runAsync<bool>(
                alive,
                [it] {
                    api::removeLibrary(it.sourceId, it.url);
                    return true;
                },
                [this](bool) { reload(); });
        });
        d->open();
    };
    this->addView(grid);
    reload();
}

void LibraryTab::willAppear(bool resetState) {
    TabBase::willAppear(resetState);
    if (appeared) reload();
    appeared = true;
}

void LibraryTab::reload() {
    grid->showLoading();
    runAsync<json>(
        alive, [] { return api::library(); },
        [this](json arr) {
            grid->focusFallback = this->getParent();
            bool refocus = grid->clear();
            auto items = gridFromAnimeArray(arr);
            grid->hideStatus();
            grid->append(items);
            if (items.empty())
                grid->showMessage(tr("La libreria e' vuota.\nApri un anime e scegli \"Aggiungi alla libreria\"."));
            if (refocus && !items.empty()) brls::Application::giveFocus(grid);
        },
        [this](const std::string& err) { grid->showMessage(err); });
}

// ============================================================================ Sorgenti

SourcesTab::SourcesTab() {
    list = new brls::Box(brls::Axis::COLUMN);
    list->setPadding(20, 40, 30, 40);
    list->addView(header(tr("Caricamento...")));
    this->addView(scrollOf(list));
    reload();
}

void SourcesTab::willAppear(bool resetState) {
    TabBase::willAppear(resetState);
    if (appeared) reload();  // le fonti attive possono essere cambiate
    appeared = true;
}

static std::string langLabel(const std::string& l) { return i18n::languageName(l); }

void SourcesTab::reload() {
    runAsync<json>(
        alive, [] { return api::sources(); },
        [this](json arr) {
            bool hadFocus = list->isChildFocused();
            list->clearViews();
            if (arr.empty()) {
                list->addView(header(tr("Nessuna fonte attiva: scegline qualcuna in Impostazioni.")));
                return;
            }
            list->addView(header(tr("Scegli dove cercare gli anime")));
            for (auto& s : arr) {
                auto* cell = new brls::DetailCell();
                std::string lang = s.value("lang", "");
                cell->setText(s.value("name", ""));
                cell->setDetailText(langLabel(lang) + (s.value("nsfw", false) ? " \xC2\xB7 18+" : ""));
                std::string id = s.value("id", ""), name = s.value("name", "");
                bool latest = s.value("supportsLatest", false);
                cell->registerClickAction([id, name, latest](brls::View*) {
                    brls::Application::pushActivity(new BrowseActivity(id, name, latest));
                    return true;
                });
                list->addView(cell);
            }
            if (hadFocus) brls::Application::giveFocus(list);
        },
        [this](const std::string& err) {
            list->clearViews();
            list->addView(header(err));
        });
}

// ============================================================================ Ricerca globale

SearchTab::SearchTab(bool askNow) {
    button = new brls::Button();
    button->setText(tr("Cerca un anime in tutte le sorgenti"));
    button->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    button->setMargins(20, 40, 0, 40);
    button->registerClickAction([this](brls::View*) {
        ask();
        return true;
    });
    this->addView(button);

    grid = new AnimeGrid(5, 206);
    grid->focusFallback = button;
    grid->onSelect = [](const GridItem& it) {
        brls::Application::pushActivity(new AnimeActivity(it.sourceId, it.url, it.title, it.thumbnail));
    };
    this->addView(grid);

    if (askNow) {
        std::weak_ptr<bool> weak = alive;
        brls::delay(400, [this, weak] {
            auto a = weak.lock();
            if (a && *a) ask();
        });
    }
}

void SearchTab::ask() {
    brls::Application::getImeManager()->openForText(
        [this](std::string text) {
            if (!text.empty()) search(text);
        },
        tr("Cerca anime"), tr("Titolo (es. One Piece)"), 64, query);
}

void SearchTab::search(const std::string& q) {
    query = q;
    int gen = ++generation;
    grid->clear();
    grid->showMessage(tr("Cerco \"{}\"...", q));
    button->setText(tr("Risultati per \"{}\" (premi per cambiare)", q));

    runAsync<json>(
        alive, [] { return api::sources(); },
        [this, gen, q](json sources) {
            if (gen != generation) return;
            pending = (int)sources.size();
            if (pending == 0) grid->showMessage(tr("Nessuna sorgente installata"));
            for (auto& s : sources) {
                std::string id = s.value("id", ""), name = s.value("name", "");
                runAsync<json>(
                    alive, [id, q] { return api::browse(id, "search", 1, q); },
                    [this, gen, name](json r) {
                        if (gen != generation) return;
                        pending--;
                        std::vector<GridItem> items;
                        int n = 0;
                        for (auto& a : r.value("animes", json::array())) {
                            if (n++ >= 8) break;  // max 8 risultati per sorgente
                            items.push_back({a.value("sourceId", ""), a.value("url", ""), a.value("title", ""), name,
                                             a.value("thumbnail", ""), a});
                        }
                        if (!items.empty()) grid->hideStatus();
                        grid->append(items);
                        if (pending == 0 && grid->count() == 0) grid->showMessage(tr("Nessun risultato"));
                    },
                    [this, gen](const std::string&) {
                        if (gen != generation) return;
                        pending--;
                        if (pending == 0 && grid->count() == 0) grid->showMessage(tr("Nessun risultato"));
                    });
            }
        },
        [this](const std::string& err) { grid->showMessage(err); });
}

// ============================================================================ Impostazioni

SettingsTab::SettingsTab() {
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setPadding(20, 40, 30, 40);
    auto& cfg = Config::instance();

    box->addView(header(tr("Fonti")));
    auto* pick = new brls::DetailCell();
    pick->setText(tr("Scegli le fonti attive"));
    pick->setDetailText(tr("{} attive", std::to_string(api::sources().size())));
    pick->registerClickAction([pick](brls::View*) {
        // dopo la conferma aggiorna subito il numero di fonti attive
        brls::Application::pushActivity(new SourcePickerActivity(false, [pick] {
            pick->setDetailText(tr("{} attive", std::to_string(api::sources().size())));
        }));
        return true;
    });
    box->addView(pick);

    box->addView(header(tr("Indirizzi dei siti attivi (cambiali se una fonte smette di funzionare)")));
    for (auto& s : src::all()) {
        if (!cfg.isSourceEnabled(s->id())) continue;
        auto* cell = new brls::InputCell();
        std::string id = s->id();
        std::string current = cfg.domains.count(id) ? cfg.domains[id] : "";
        cell->init(
            s->name(), current.empty() ? s->defaultBaseUrl() : current,
            [id, cell](std::string text) {
                auto s = src::byId(id);
                if (text.empty() || (s && text == s->defaultBaseUrl()))
                    Config::instance().domains.erase(id);
                else
                    Config::instance().domains[id] = text;
                Config::instance().save();
                Config::instance().applyDomains();
                if (s) cell->setValue(s->baseUrl());
            },
            "", tr("Es. https://www.animeworld.ac (lascia vuoto per il predefinito)"), 80);
        box->addView(cell);
    }

    box->addView(header(tr("Riproduzione")));

    auto* hw = new brls::BooleanCell();
    hw->init(tr("Decodifica hardware"), cfg.hardwareDecoding, [](bool on) {
        Config::instance().hardwareDecoding = on;
        Config::instance().save();
    });
    box->addView(hw);

    {
        // lingua dei sottotitoli quando il video ne ha piu' di una
        std::vector<std::string> labels;
        int selected = 0;
        const auto& codes = sublang::choices();
        for (size_t i = 0; i < codes.size(); i++) {
            const std::string& c = codes[i];
            labels.push_back(c == "auto"  ? tr("Lingua della console")
                             : c == "off" ? tr("Nessuno (sottotitoli spenti)")
                                          : i18n::languageName(c));
            if (c == cfg.subtitleLang) selected = (int)i;
        }
        auto* subLang = new brls::SelectorCell();
        subLang->init(tr("Lingua dei sottotitoli"), labels, selected, [](int i) {
            const auto& codes = sublang::choices();
            if (i < 0 || i >= (int)codes.size()) return;
            Config::instance().subtitleLang = codes[i];
            Config::instance().save();
        });
        box->addView(subLang);
    }

    auto* proxy = new brls::BooleanCell();
    proxy->init(tr("Ripara gli stream con segmenti camuffati (proxy locale)"), cfg.hlsProxy, [](bool on) {
        Config::instance().hlsProxy = on;
        Config::instance().save();
    });
    box->addView(proxy);

    auto* skip = new brls::BooleanCell();
    skip->init(tr("Salta automaticamente la sigla (se la fonte la indica)"), cfg.autoSkipOpening, [](bool on) {
        Config::instance().autoSkipOpening = on;
        Config::instance().save();
    });
    box->addView(skip);

    box->addView(header(tr("Informazioni")));
    auto* fwd = new brls::DetailCell();
    fwd->setText(tr("Icona nella schermata Home (forwarder)"));
    fwd->setDetailText("Sphaira");
    fwd->registerClickAction([](brls::View*) {
        auto* d = new brls::Dialog(tr(
            "Puoi avviare AnikkuNX direttamente dalla schermata Home della console creando un forwarder con Sphaira:\n\n"
            "1. Apri Sphaira (il menu homebrew).\n"
            "2. Seleziona AnikkuNX e premi X.\n"
            "3. Scegli \"Installa forwarder\" (Install Forwarder) e conferma.\n\n"
            "L'icona di AnikkuNX comparira' nella Home insieme ai giochi."));
        d->addButton(tr("OK"), [] {});
        d->open();
        return true;
    });
    box->addView(fwd);

    auto* ver = new brls::DetailCell();
    ver->setText(tr("Versione"));
    ver->setDetailText("AnikkuNX v" + updater::currentVersion());
    box->addView(ver);

    auto* upd = new brls::DetailCell();
    upd->setText(tr("Controlla aggiornamenti"));
    upd->setDetailText("github.com/" UPDATE_REPO_DISPLAY);
    upd->registerClickAction([](brls::View*) {
        // (debug interno: con L+R premuti si apre l'invio del .nro dal PC)
        if (debugComboHeld())
            brls::Application::pushActivity(new DebugUploadActivity());
        else
            checkForUpdates(true);
        return true;
    });
    box->addView(upd);

    auto* autoUpd = new brls::BooleanCell();
    autoUpd->init(tr("Controlla aggiornamenti all'avvio"), cfg.checkUpdates, [](bool on) {
        Config::instance().checkUpdates = on;
        Config::instance().save();
    });
    box->addView(autoUpd);

    auto* newEps = new brls::BooleanCell();
    newEps->init(tr("Controlla i nuovi episodi della libreria all'avvio"), cfg.checkNewEpisodes, [](bool on) {
        Config::instance().checkNewEpisodes = on;
        Config::instance().save();
    });
    box->addView(newEps);

    auto* about = new brls::Label();
    about->setText(tr(
        "App autonoma per Nintendo Switch con fonti italiane, inglesi e multilingua "
        "(porting delle estensioni di Anikku/Aniyomi).\n"
        "Libreria e progressi sono salvati in sdmc:/switch/AnikkuNX. Avvia l'app tenendo premuto R su un gioco "
        "per avere piu' memoria."));
    about->setFontSize(15);
    about->setTextColor(nvgRGB(150, 150, 160));
    about->setMarginTop(24);
    box->addView(about);

    this->addView(scrollOf(box));
}
