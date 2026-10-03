#include "activity/anime_activity.hpp"
#include "util/i18n.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "activity/player_activity.hpp"
#include "app/api.hpp"
#include "app/downloads.hpp"
#include "view/anime_grid.hpp"
#include "view/crunchy.hpp"

#include <borealis/extern/nanovg/stb_image.h>
#include <cstring>

using json = nlohmann::json;

static std::string fmtTime(double s) {
    long t = (long)s;
    char buf[24];
    snprintf(buf, sizeof(buf), "%02ld:%02ld", t / 60, t % 60);
    return buf;
}

static std::string upper(std::string s) {
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

static std::string clean(std::string s) {
    for (auto& c : s)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    std::string out;
    for (char c : s)
        if (!(c == ' ' && !out.empty() && out.back() == ' ')) out += c;
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out;
}

static std::string epTag(const json& ep) {
    double n = ep.value("number", -1.0);
    if (n < 0) return "";
    char buf[24];
    snprintf(buf, sizeof(buf), "E%g", n);
    return buf;
}

static const int COLS = 5;
static const float CARD_W = 218, CARD_GAP = 19, THUMB_H = 123;
static const float ROW_H = 262, HERO_H = 530;
static int lastColumn = 0;  // keeps the column when moving between grid rows

// ----------------------------------------------------------------------------- header

/**
 * Title logo drawn to fit its box, anchored bottom-left. TVDB "clear logos" often sit in the
 * middle of a big transparent canvas, so the image is decoded here and cropped to its visible part.
 */
class LogoImage : public brls::Image {
  public:
    LogoImage() { setBackgroundColor(nvgRGBA(0, 0, 0, 0)); }
    ~LogoImage() override { *alive = false; }

    void load(const std::string& url) {
        current = url;
        clear();
        if (url.empty()) return;
        std::string u = url;
        runAsync<std::shared_ptr<Pixels>>(
            alive,
            [u] {
                std::string data = api::download(u);
                int w = 0, h = 0, c = 0;
                unsigned char* px = stbi_load_from_memory((const unsigned char*)data.data(), (int)data.size(), &w, &h, &c, 4);
                if (!px) throw std::runtime_error("logo decode failed");
                int x0 = w, y0 = h, x1 = -1, y1 = -1;
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                        if (px[((size_t)y * w + x) * 4 + 3] > 16) {
                            x0 = std::min(x0, x);
                            x1 = std::max(x1, x);
                            y0 = std::min(y0, y);
                            y1 = std::max(y1, y);
                        }
                if (x1 < x0) x0 = 0, y0 = 0, x1 = w - 1, y1 = h - 1;
                auto out = std::make_shared<Pixels>();
                out->w = x1 - x0 + 1;
                out->h = y1 - y0 + 1;
                out->rgba.resize((size_t)out->w * out->h * 4);
                for (int y = 0; y < out->h; y++)
                    memcpy(&out->rgba[(size_t)y * out->w * 4], px + ((size_t)(y0 + y) * w + x0) * 4, (size_t)out->w * 4);
                stbi_image_free(px);
                return out;
            },
            [this, u](std::shared_ptr<Pixels> p) {
                if (u != current) return;
                int tex = nvgCreateImageRGBA(brls::Application::getNVGContext(), p->w, p->h, 0, p->rgba.data());
                if (tex) innerSetImage(tex);
            },
            [](const std::string&) {});
    }

    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) override {
        int tex = getTexture();
        float iw = getOriginalImageWidth(), ih = getOriginalImageHeight();
        if (!tex || iw <= 0 || ih <= 0) return;
        float s = std::min(w / iw, h / ih);
        float dw = iw * s, dh = ih * s, ox = x, oy = y + h - dh;
        nvgBeginPath(vg);
        nvgRect(vg, ox, oy, dw, dh);
        nvgFillPaint(vg, nvgImagePattern(vg, ox, oy, dw, dh, 0, tex, 1.0f));
        nvgFill(vg);
    }

  private:
    struct Pixels {
        int w = 0, h = 0;
        std::vector<unsigned char> rgba;
    };
    AliveToken alive = makeAlive();
    std::string current;
};

class SeriesHero : public brls::RecyclerCell {
  public:
    explicit SeriesHero(AnimeActivity* a) : act(a) {
        setAxis(brls::Axis::COLUMN);
        setHeight(HERO_H);
        setPadding(30, 56, 6, 56);
        setLineColor(nvgRGBA(0, 0, 0, 0));
        setAlignItems(brls::AlignItems::STRETCH);

        art = new cr::HeroBackdrop();
        art->dim = 110;
        art->leftFade = 0.8f;
        addView(art);

        auto* titleBox = new brls::Box(brls::Axis::COLUMN);
        titleBox->setHeight(140);
        titleBox->setJustifyContent(brls::JustifyContent::FLEX_END);
        titleBox->setAlignItems(brls::AlignItems::FLEX_START);
        logo = new LogoImage();
        logo->setWidth(500);
        logo->setHeight(136);
        logo->setVisibility(brls::Visibility::GONE);
        titleBox->addView(logo);
        titleText = cr::label("", 44, cr::text(), true);
        titleText->setWidth(680);
        titleBox->addView(titleText);
        addView(titleBox);

        kicker = cr::label(" ", 15, cr::orange(), true);
        kicker->setMarginTop(14);
        addView(kicker);
        meta = cr::label(" ", 15, nvgRGB(200, 200, 206));
        meta->setMarginTop(8);
        addView(meta);

        auto* buttons = new brls::Box(brls::Axis::ROW);
        buttons->setAlignItems(brls::AlignItems::CENTER);
        buttons->setMarginTop(18);
        watch = new cr::CtaButton("START WATCHING", true, true, [this] { act->playContinue(); });
        watch->setWidth(280);
        buttons->addView(watch);
        listBtn = new brls::Box(brls::Axis::ROW);
        listBtn->setFocusable(true);
        listBtn->setWidth(46);
        listBtn->setHeight(46);
        listBtn->setMarginLeft(12);
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
            act->toggleLibrary();
            return true;
        });
        listBtn->addGestureRecognizer(new brls::TapGestureRecognizer(listBtn));
        buttons->addView(listBtn);
        addView(buttons);

        auto* lower = new brls::Box(brls::Axis::ROW);
        lower->setMarginTop(22);
        lower->setAlignItems(brls::AlignItems::FLEX_START);
        desc = cr::label("", 15, nvgRGB(220, 220, 225));
        desc->setWidth(640);
        lower->addView(desc);
        auto* gap = new brls::Box(brls::Axis::ROW);
        gap->setGrow(1);
        lower->addView(gap);
        details = cr::label("", 14, cr::muted());
        details->setWidth(380);
        lower->addView(details);
        addView(lower);

        auto* spacer = new brls::Box(brls::Axis::COLUMN);
        spacer->setGrow(1);
        addView(spacer);

        auto* sep = new brls::Box(brls::Axis::ROW);
        sep->setHeight(1);
        sep->setBackgroundColor(nvgRGB(60, 60, 68));
        addView(sep);

        auto* head = new brls::Box(brls::Axis::ROW);
        head->setAlignItems(brls::AlignItems::CENTER);
        head->setMarginTop(14);
        head->setHeight(44);
        listTitle = cr::label("Episodes", 22, cr::text(), true);
        head->addView(listTitle);
        count = cr::label("", 16, cr::muted());
        count->setMarginLeft(10);
        head->addView(count);
        status = cr::label("", 14, cr::dim());
        status->setMarginLeft(22);
        head->addView(status);
        auto* gap2 = new brls::Box(brls::Axis::ROW);
        gap2->setGrow(1);
        head->addView(gap2);
        sortBtn = new cr::CtaButton("NEWEST FIRST", false, false, [this] { act->toggleOrder(); });
        sortBtn->setHeight(38);
        sortBtn->setWidth(170);
        head->addView(sortBtn);
        addView(head);
    }

    brls::View* getDefaultFocus() override {
        if (lastChild && lastChild->getVisibility() == brls::Visibility::VISIBLE) return lastChild;
        return watch;
    }

    void onChildFocusGained(brls::View* directChild, brls::View* focusedView) override {
        brls::RecyclerCell::onChildFocusGained(directChild, focusedView);
        lastChild = focusedView;
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override {
        // show the logo instead of the text title once it has actually loaded
        bool logoReady = logo->getVisibility() == brls::Visibility::VISIBLE && logo->getTexture() != 0;
        if (logoReady && titleText->getVisibility() == brls::Visibility::VISIBLE)
            titleText->setVisibility(brls::Visibility::GONE);
        brls::RecyclerCell::draw(vg, x, y, width, height, style, ctx);
    }

    void update() {
        auto info = act->info;
        std::string artUrl = act->thumbnail;
        if (info && !info->fanart.empty())
            artUrl = info->fanart;
        else if (info && !info->banner.empty())
            artUrl = info->banner;
        bool crisp = artUrl != act->thumbnail;
        art->blur = !crisp;
        art->focusY = crisp ? 0.3f : 0.22f;
        art->dim = crisp ? 70 : 120;
        if (artUrl != shownArt) {
            shownArt = artUrl;
            art->setUrl(artUrl);
        }

        std::string logoUrl = info ? info->logo : "";
        if (logoUrl != shownLogo) {
            shownLogo = logoUrl;
            logo->load(logoUrl);
            logo->setVisibility(logoUrl.empty() ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
            titleText->setVisibility(brls::Visibility::VISIBLE);
        }
        titleText->setText(cr::wrap(act->title, 44, 680, 2));

        std::string src = act->data.is_object() ? act->data.value("sourceName", std::string()) : "";
        kicker->setText(src.empty() ? " " : upper(src));

        std::string m;
        auto add = [&m](const std::string& s) {
            if (!s.empty()) m += (m.empty() ? "" : "   \xC2\xB7   ") + s;
        };
        if (info && info->score > 0) {
            int stars = (int)std::lround(info->score / 20.0);
            std::string st;
            for (int i = 0; i < 5; i++) st += i < stars ? "\xE2\x98\x85" : "\xE2\x98\x86";
            char buf[48];
            snprintf(buf, sizeof(buf), "  %.1f", info->score / 20.0);
            add(st + buf);
        }
        std::string genres;
        if (info && !info->genres.empty()) {
            for (size_t i = 0; i < info->genres.size() && i < 5; i++) genres += (i ? ", " : "") + info->genres[i];
        } else if (act->data.is_object() && act->data["genre"].is_string()) {
            genres = clean(act->data["genre"].get<std::string>());
        }
        add(cr::ellipsize(genres, 70));
        if (act->data.is_object() && act->data["status"].is_string()) add(act->data["status"].get<std::string>());
        meta->setText(m.empty() ? " " : m);

        std::string d = info && !info->description.empty() ? info->description
                        : act->data.is_object() && act->data["description"].is_string()
                            ? act->data["description"].get<std::string>()
                            : "";
        desc->setText(cr::wrap(clean(d), 15, 638, 4));

        std::string det;
        auto line = [&det](const std::string& k, const std::string& v) {
            if (!v.empty()) det += (det.empty() ? "" : "\n") + k + ":  " + v;
        };
        line("Source", src);
        if (info && !info->englishTitle.empty() && info->englishTitle != act->title) line("Also known as", info->englishTitle);
        if (act->loadedOnce) line(act->seasons() ? "Seasons" : "Episodes", std::to_string(act->items().size()));
        if (act->data.is_object() && act->data["author"].is_string()) line("Studio", act->data["author"].get<std::string>());
        details->setText(cr::wrap(det, 14, 378, 5));

        listIcon->setIcon(act->inLibrary ? cr::ICON_BOOKMARK : cr::ICON_BOOKMARK_BORDER);

        bool seasons = act->seasons();
        size_t n = act->items().size();
        listTitle->setText(seasons ? "Seasons" : "Episodes");
        count->setText(act->loadedOnce ? std::to_string(n) : "");
        if (!act->error.empty())
            status->setText(cr::ellipsize("Couldn't load: " + act->error + " (Y to retry)", 70));
        else if (!act->loadedOnce)
            status->setText("Loading episodes\xE2\x80\xA6");
        else if (n == 0)
            status->setText("No episodes available");
        else
            status->setText(seasons ? "" : "Press + on an episode for more options");
        sortBtn->setText(act->oldestFirst ? "OLDEST FIRST" : "NEWEST FIRST");
        sortBtn->setVisibility(seasons || n == 0 ? brls::Visibility::GONE : brls::Visibility::VISIBLE);

        int ci = seasons ? -1 : act->continueIndex();
        if (ci >= 0) {
            const auto& ep = act->items()[ci];
            std::string tag = epTag(ep);
            bool anyWatched = false;
            for (auto& e : act->items())
                if (e.value("watched", false) || e.value("position", 0.0) > 5) anyWatched = true;
            std::string verb = ep.value("position", 0.0) > 5 ? "CONTINUE" : anyWatched ? "WATCH NEXT" : "START WATCHING";
            watch->setText(tag.empty() ? verb : verb + " " + tag);
        } else {
            watch->setText(seasons ? "PICK A SEASON" : "START WATCHING");
        }
    }

    cr::HeroBackdrop* art;

  private:
    AnimeActivity* act;
    LogoImage* logo;
    brls::Label *titleText, *kicker, *meta, *desc, *details, *listTitle, *count, *status;
    cr::CtaButton *watch, *sortBtn;
    brls::Box* listBtn;
    cr::IconView* listIcon;
    brls::View* lastChild = nullptr;
    std::string shownArt, shownLogo;
};

// ----------------------------------------------------------------------------- episode cards

/** 16:9 episode still with runtime badge and progress; falls back to the show's art and the episode number. */
class EpisodeThumb : public CoverImage {
  public:
    brls::Image* fallback = nullptr;
    std::string number;
    float progress = -1;
    bool watched = false;
    bool hasImage = false;
    int runtime = 0;

    EpisodeThumb() {
        setWidth(CARD_W);
        setHeight(THUMB_H);
        setBackgroundColor(nvgRGBA(0, 0, 0, 0));
    }

    void setImage(const std::string& url) {
        if (url == imageUrl) return;
        imageUrl = url;
        hasImage = !url.empty();
        clear();
        setUrl(url);
    }

    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx) override {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, w, h);
        nvgFillColor(vg, nvgRGB(35, 37, 43));
        nvgFill(vg);
        if (hasImage && getTexture()) {
            CoverImage::draw(vg, x, y, w, h, style, ctx);
        } else {
            int tex = fallback ? fallback->getTexture() : 0;
            float iw = fallback ? fallback->getOriginalImageWidth() : 0, ih = fallback ? fallback->getOriginalImageHeight() : 0;
            if (tex && iw > 0 && ih > 0) {
                float s = std::max(w / iw, h / ih);
                float dw = iw * s, dh = ih * s;
                NVGpaint p = nvgImagePattern(vg, x + (w - dw) / 2, y + (h - dh) * 0.3f, dw, dh, 0, tex, 1.0f);
                nvgBeginPath(vg);
                nvgRect(vg, x, y, w, h);
                nvgFillPaint(vg, p);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgRect(vg, x, y, w, h);
                nvgFillColor(vg, nvgRGBA(0, 0, 0, 150));
                nvgFill(vg);
            }
            nvgFontFaceId(vg, brls::Application::getDefaultFont());
            nvgFontSize(vg, 30);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgText(vg, x + w / 2, y + h / 2, number.c_str(), nullptr);
            nvgText(vg, x + w / 2 + 0.8f, y + h / 2, number.c_str(), nullptr);
        }
        if (watched) {
            nvgBeginPath(vg);
            nvgRect(vg, x, y, w, h);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 110));
            nvgFill(vg);
        }
        if (runtime > 0) {
            std::string t = std::to_string(runtime) + "m";
            nvgFontFaceId(vg, brls::Application::getDefaultFont());
            nvgFontSize(vg, 13);
            float b[4];
            nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, b);
            float bw = b[2] - b[0] + 10, bh = 20, bx = x + w - bw - 6, by = y + h - bh - 8;
            nvgBeginPath(vg);
            nvgRect(vg, bx, by, bw, bh);
            nvgFillColor(vg, nvgRGBA(0, 0, 0, 190));
            nvgFill(vg);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, bx + bw / 2, by + bh / 2 + 1, t.c_str(), nullptr);
        }
        float p = watched ? 1.0f : progress;
        if (p > 0) {
            nvgBeginPath(vg);
            nvgRect(vg, x, y + h - 4, w, 4);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, 60));
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgRect(vg, x, y + h - 4, w * std::min(1.0f, p), 4);
            nvgFillColor(vg, cr::orange());
            nvgFill(vg);
        }
    }

  private:
    std::string imageUrl = "\x01";
};

/** Small round download-state badge: icon (to download, queued, done, error) or a percentage. */
class DownloadBadge : public brls::View {
  public:
    std::string glyph = cr::icon(cr::ICON_DOWNLOAD);
    bool isIcon = true;
    NVGcolor bg = nvgRGBA(255, 255, 255, 26);
    NVGcolor fg = nvgRGB(220, 220, 226);
    DownloadBadge() {
        setWidth(30);
        setHeight(30);
    }
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) override {
        nvgBeginPath(vg);
        nvgCircle(vg, x + w / 2, y + h / 2, w / 2);
        nvgFillColor(vg, bg);
        nvgFill(vg);
        int f = isIcon ? brls::Application::getFont(brls::FONT_MATERIAL_ICONS) : brls::Application::getDefaultFont();
        if (f < 0) return;
        nvgFontFaceId(vg, f);
        nvgFontSize(vg, isIcon ? 18 : 11);
        nvgFillColor(vg, fg);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + w / 2, y + h / 2, glyph.c_str(), nullptr);
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
    }
};

class EpisodeCard : public brls::Box {
  public:
    EpisodeCard(AnimeActivity* a, int column) : brls::Box(brls::Axis::COLUMN), act(a), col(column) {
        setWidth(CARD_W);
        if (column < COLS - 1) setMarginRight(CARD_GAP);
        setFocusable(true);
        setHideHighlightBackground(true);
        setHighlightCornerRadius(3);
        setHighlightPadding(6);

        thumb = new EpisodeThumb();
        addView(thumb);
        kicker = cr::label(" ", 11, cr::dim(), true);
        kicker->setSingleLine(true);
        kicker->setWidth(CARD_W);
        kicker->setMarginTop(10);
        addView(kicker);
        title = cr::label(" ", 15, cr::text(), true);
        title->setWidth(CARD_W);
        title->setMarginTop(4);
        addView(title);
        auto* subRow = new brls::Box(brls::Axis::ROW);
        subRow->setAlignItems(brls::AlignItems::CENTER);
        subRow->setMarginTop(6);
        sub = cr::label(" ", 13, cr::muted());
        sub->setSingleLine(true);
        sub->setGrow(1);
        subRow->addView(sub);
        dl = new DownloadBadge();
        subRow->addView(dl);
        addView(subRow);

        registerClickAction([this](brls::View*) {
            if (index < 0) return true;
            if (act->seasons())
                act->openSeason(index);
            else
                act->playEpisode(index);
            return true;
        });
        registerAction("Quality", brls::BUTTON_X, [this](brls::View*) {
            if (index >= 0 && !act->seasons()) act->chooseVideo(index);
            return true;
        });
        registerAction("Download", brls::BUTTON_RB, [this](brls::View*) {
            if (index >= 0 && !act->seasons()) act->downloadEpisode(index);
            return true;
        });
        registerAction("Options", brls::BUTTON_START, [this](brls::View*) {
            if (index >= 0 && !act->seasons()) act->episodeOptions(index);
            return true;
        });
        addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus st, brls::Sound* snd) {
            if (st.state != brls::GestureState::END || index < 0) return;
            *snd = brls::SOUND_CLICK;
            auto f = dl->getFrame();
            bool onBadge = dl->getVisibility() == brls::Visibility::VISIBLE && st.position.x >= f.getMinX() - 10 &&
                           st.position.y >= f.getMinY() - 10;
            if (act->seasons())
                act->openSeason(index);
            else if (onBadge)
                act->downloadEpisode(index);
            else
                act->playEpisode(index);
        }));
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        lastColumn = col;
    }

    void bind(int i, brls::Image* fallbackArt) {
        const json& items = act->items();
        if (i < 0 || i >= (int)items.size()) {
            index = -1;
            setVisibility(brls::Visibility::INVISIBLE);
            setFocusable(false);
            episodeUrl.clear();
            return;
        }
        index = i;
        setVisibility(brls::Visibility::VISIBLE);
        setFocusable(true);
        thumb->fallback = fallbackArt;
        const json& ep = items[i];
        auto info = act->info;
        std::string series = info && !info->englishTitle.empty() ? info->englishTitle : act->title;
        kicker->setText(upper(cr::ellipsize(series, 34)));

        if (act->seasons()) {
            thumb->number = "S" + std::to_string(i + 1);
            thumb->setImage("");
            thumb->progress = -1;
            thumb->watched = false;
            thumb->runtime = 0;
            title->setText(cr::wrap(ep.value("title", ""), 15, CARD_W - 2, 2));
            sub->setText(" ");
            dl->setVisibility(brls::Visibility::GONE);
            episodeUrl.clear();
            return;
        }

        double num = ep.value("number", -1.0);
        std::string tag = epTag(ep);
        thumb->number = tag.empty() ? "#" + std::to_string(i + 1) : tag;
        const meta::Episode* m = nullptr;
        if (info && num > 0 && std::floor(num) == num) {
            auto it = info->episodes.find((int)num);
            if (it != info->episodes.end()) m = &it->second;
        }
        thumb->setImage(m ? m->image : "");
        thumb->runtime = m ? m->runtime : 0;
        bool w = ep.value("watched", false);
        double pos = ep.value("position", 0.0), dur = ep.value("duration", 0.0);
        thumb->watched = w;
        thumb->progress = dur > 0 && pos > 5 ? (float)(pos / dur) : -1;

        std::string name = clean(ep.value("name", std::string()));
        std::string lname = name;
        for (auto& c : lname) c = (char)tolower((unsigned char)c);
        bool generic = name.empty() || lname.rfind("ep", 0) == 0 || lname.find_first_not_of("0123456789. ") == std::string::npos;
        std::string label;
        if (m && !m->title.empty())
            label = (tag.empty() ? "" : tag + " - ") + m->title;
        else if (generic)
            label = tag.empty() ? (name.empty() ? "Episode " + std::to_string(i + 1) : name) : "Episode " + tag.substr(1);
        else
            label = (tag.empty() ? "" : tag + " - ") + name;
        title->setText(cr::wrap(label, 15, CARD_W - 2, 2));

        std::string detail = " ";
        if (w)
            detail = "Watched";
        else if (pos > 5)
            detail = fmtTime(pos) + " / " + fmtTime(dur);
        else if (ep.value("filler", false))
            detail = "Filler";
        sub->setText(detail);
        sub->setTextColor(w ? cr::orange() : cr::muted());
        dl->setVisibility(brls::Visibility::VISIBLE);
        episodeUrl = ep.value("url", "");
        lastText.clear();
        refreshDownload();
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override {
        // lo stato del download cambia in background: controllato ogni mezzo secondo
        auto now = std::chrono::steady_clock::now();
        if (now - lastCheck > std::chrono::milliseconds(500)) refreshDownload();
        brls::Box::draw(vg, x, y, width, height, style, ctx);
    }

  private:
    void refreshDownload() {
        lastCheck = std::chrono::steady_clock::now();
        if (episodeUrl.empty()) return;
        double progress = 0;
        std::string st = downloads::status(act->sourceId, episodeUrl, &progress);
        std::string text;
        bool icon = true;
        NVGcolor bg = nvgRGBA(255, 255, 255, 26), fg = nvgRGB(220, 220, 226);
        if (st == "done") {
            text = cr::icon(cr::ICON_DONE);
            bg = cr::orange();
            fg = nvgRGB(0, 0, 0);
        } else if (st == "downloading") {
            text = std::to_string((int)(progress * 100)) + "%";
            icon = false;
            bg = nvgRGBA(244, 117, 33, 90);
        } else if (st == "queued") {
            text = cr::icon(cr::ICON_QUEUED);
        } else if (st == "failed") {
            text = cr::icon(cr::ICON_ERROR);
            bg = nvgRGBA(200, 60, 60, 140);
        } else {
            text = cr::icon(cr::ICON_DOWNLOAD);
        }
        if (text != lastText) {
            lastText = text;
            dl->glyph = text;
            dl->isIcon = icon;
            dl->bg = bg;
            dl->fg = fg;
        }
    }

    AnimeActivity* act;
    int col;
    int index = -1;
    EpisodeThumb* thumb;
    brls::Label *kicker, *title, *sub;
    DownloadBadge* dl;
    std::string episodeUrl, lastText;
    std::chrono::steady_clock::time_point lastCheck{};
};

class EpisodeRow : public brls::RecyclerCell {
  public:
    int boundRow = -1;

    explicit EpisodeRow(AnimeActivity* a) : act(a) {
        setAxis(brls::Axis::ROW);
        setHeight(ROW_H);
        setPadding(12, 0, 0, (1280 - (COLS * CARD_W + (COLS - 1) * CARD_GAP)) / 2);
        setAlignItems(brls::AlignItems::FLEX_START);
        setLineColor(nvgRGBA(0, 0, 0, 0));
        for (int c = 0; c < COLS; c++) {
            cards[c] = new EpisodeCard(a, c);
            addView(cards[c]);
        }
    }

    void bind(int row) {
        boundRow = row;
        brls::Image* art = act->hero ? act->hero->art : nullptr;
        for (int c = 0; c < COLS; c++) cards[c]->bind(row * COLS + c, art);
    }

    brls::View* getDefaultFocus() override {
        for (int c = std::min(lastColumn, COLS - 1); c >= 0; c--)
            if (cards[c]->isFocusable()) return cards[c];
        return nullptr;
    }

  private:
    AnimeActivity* act;
    EpisodeCard* cards[COLS];
};

// ----------------------------------------------------------------------------- data source

/** Recycler that starts at the top: borealis centres the first row, which hides half of the tall header. */
class SeriesRecycler : public brls::RecyclerFrame {
  public:
    void onLayout() override {
        bool first = !laidOut;
        brls::RecyclerFrame::onLayout();
        if (first && getWidth() > 0) {
            laidOut = true;
            setContentOffsetY(0, false);
        }
    }

  private:
    bool laidOut = false;
};

class EpisodeDataSource : public brls::RecyclerDataSource {
  public:
    explicit EpisodeDataSource(AnimeActivity* a) : act(a) {}

    json items = json::array();
    bool seasons = false;

    int numberOfSections(brls::RecyclerFrame*) override { return 1; }
    int numberOfRows(brls::RecyclerFrame*, int) override { return 1 + ((int)items.size() + COLS - 1) / COLS; }
    float heightForRow(brls::RecyclerFrame*, brls::IndexPath index) override { return index.row == 0 ? HERO_H : ROW_H; }

    brls::RecyclerCell* cellForRow(brls::RecyclerFrame* recycler, brls::IndexPath index) override {
        if (index.row == 0) {
            auto* hero = (SeriesHero*)recycler->dequeueReusableCell("Hero");
            hero->update();
            return hero;
        }
        auto* row = (EpisodeRow*)recycler->dequeueReusableCell("Row");
        row->bind(index.row - 1);
        return row;
    }

  private:
    AnimeActivity* act;
};

// ----------------------------------------------------------------------------- activity

AnimeActivity::AnimeActivity(std::string sid, std::string u, std::string t, std::string thumb)
    : sourceId(std::move(sid)), url(std::move(u)), title(std::move(t)), thumbnail(std::move(thumb)) {}

AnimeActivity::~AnimeActivity() { *alive = false; }

const json& AnimeActivity::items() const { return dataSource->items; }

bool AnimeActivity::seasons() const { return dataSource->seasons; }

brls::View* AnimeActivity::createContentView() {
    recycler = new SeriesRecycler();
    recycler->setScrollingIndicatorVisible(false);
    recycler->estimatedRowHeight = ROW_H;
    recycler->registerCell("Header", [] { return brls::RecyclerHeader::create(); });
    recycler->registerCell("Hero", [this] {
        hero = new SeriesHero(this);
        return (brls::RecyclerCell*)hero;
    });
    recycler->registerCell("Row", [this] {
        auto* r = new EpisodeRow(this);
        rows.push_back(r);
        return (brls::RecyclerCell*)r;
    });
    dataSource = new EpisodeDataSource(this);
    recycler->setDataSource(dataSource);

    auto* root = cr::page("", recycler);
    // Aggiorna (Y) e Inverti ordine (R3)
    root->registerAction(
        "Refresh", brls::BUTTON_Y,
        [this](brls::View*) {
            load();
            return true;
        },
        true);
    root->registerAction(
        "Reverse order", brls::BUTTON_RSB,
        [this](brls::View*) {
            toggleOrder();
            return true;
        },
        true);
    return root;
}

void AnimeActivity::onContentAvailable() {
    load();
    loadMeta();
}

void AnimeActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);
    // tornando dal player aggiorniamo i progressi
    if (loadedOnce) load(true);
}

void AnimeActivity::load(bool cached) {
    auto sid = sourceId, u = url, t = title;
    runAsync<json>(
        alive, [sid, u, t, cached] { return api::anime(sid, u, t, cached); },
        [this](json d) {
            data = std::move(d);
            loadedOnce = true;
            error.clear();
            render();
        },
        [this](const std::string& err) {
            error = err;
            if (hero) hero->update();
        });
}

void AnimeActivity::loadMeta() {
    std::string t = title;
    runAsync<std::shared_ptr<const meta::Info>>(
        alive, [t] { return meta::lookup(t); },
        [this](std::shared_ptr<const meta::Info> r) {
            if (!r || !r->found) return;
            info = r;
            refreshCells();
        });
}

void AnimeActivity::render() {
    std::string t = data.value("title", title);
    if (!t.empty()) title = t;
    if (data["thumbnail"].is_string() && !data["thumbnail"].get<std::string>().empty())
        thumbnail = data["thumbnail"].get<std::string>();
    inLibrary = data.value("inLibrary", false);

    bool s = data.value("fetchType", std::string("episodes")) == "seasons";
    dataSource->seasons = s;
    dataSource->items = s ? data.value("seasons", json::array()) : data.value("episodes", json::array());
    if (!s && oldestFirst) std::reverse(dataSource->items.begin(), dataSource->items.end());

    if (dataSource->items.size() != shownCount) {
        shownCount = dataSource->items.size();
        recycler->reloadData();
        recycler->setContentOffsetY(0, false);
    }
    refreshCells();
}

void AnimeActivity::refreshCells() {
    if (hero) hero->update();
    for (auto* r : rows)
        if (r->boundRow >= 0) r->bind(r->boundRow);
}

void AnimeActivity::toggleOrder() {
    if (!dataSource || dataSource->seasons || data.is_null()) return;
    oldestFirst = !oldestFirst;
    render();
}

void AnimeActivity::toggleLibrary() {
    bool add = !inLibrary;
    auto sid = sourceId, u = url, t = title;
    runAsync<bool>(
        alive,
        [add, sid, u, t] {
            if (add)
                api::addLibrary(sid, u, t);
            else
                api::removeLibrary(sid, u);
            return add;
        },
        [this](bool added) {
            inLibrary = added;
            if (hero) hero->update();
            brls::Application::notify(added ? "Added to My List" : "Removed from My List");
        });
}

void AnimeActivity::playContinue() {
    if (seasons()) {
        if (!items().empty()) openSeason(0);
        return;
    }
    int i = continueIndex();
    if (i >= 0) playEpisode(i);
}

void AnimeActivity::episodeOptions(int index) {
    if (index < 0 || index >= (int)dataSource->items.size()) return;
    const auto& ep = dataSource->items[index];
    std::string name = ep.value("name", std::string());
    std::string tag = epTag(ep);
    std::vector<std::string> labels = {"Play", "Choose Quality", "Download"};
    auto* dd = new brls::Dropdown(cr::ellipsize(tag.empty() ? name : tag + "  " + name, 60), labels, [](int) {}, 0,
                                  [this, index](int sel) {
                                      if (sel == 0)
                                          playEpisode(index);
                                      else if (sel == 1)
                                          chooseVideo(index);
                                      else if (sel == 2)
                                          downloadEpisode(index);
                                  });
    brls::Application::pushActivity(new brls::Activity(dd));
}

int AnimeActivity::continueIndex() const {
    const json& eps = dataSource->items;
    if (eps.empty()) return -1;
    int inProgress = -1;
    double lastWatched = -1;
    int lastWatchedIdx = -1;
    for (int i = 0; i < (int)eps.size(); i++) {
        double n = eps[i].value("number", -1.0);
        bool w = eps[i].value("watched", false);
        double pos = eps[i].value("position", 0.0);
        if (pos > 5 && !w && (inProgress < 0 || n > eps[inProgress].value("number", -1.0))) inProgress = i;
        if (w && n > lastWatched) {
            lastWatched = n;
            lastWatchedIdx = i;
        }
    }
    if (inProgress >= 0) return inProgress;
    if (lastWatchedIdx >= 0) {
        int best = -1;
        for (int i = 0; i < (int)eps.size(); i++) {
            double n = eps[i].value("number", -1.0);
            if (n > lastWatched && (best < 0 || n < eps[best].value("number", -1.0))) best = i;
        }
        if (best >= 0) return best;
        int next = oldestFirst ? lastWatchedIdx + 1 : lastWatchedIdx - 1;
        return next >= 0 && next < (int)eps.size() ? next : -1;
    }
    // mai visto: primo episodio (numero piu' basso, altrimenti l'ultimo della lista)
    int first = -1;
    for (int i = 0; i < (int)eps.size(); i++) {
        double n = eps[i].value("number", -1.0);
        if (n >= 0 && (first < 0 || n < eps[first].value("number", -1.0))) first = i;
    }
    if (first >= 0) return first;
    return oldestFirst ? 0 : (int)eps.size() - 1;
}

void AnimeActivity::playEpisode(int index, const std::string& forcedToken, double startAt) {
    PlayRequest req;
    req.sourceId = sourceId;
    req.animeUrl = url;
    req.animeTitle = title;
    req.thumbnail = thumbnail;
    for (auto& e : dataSource->items)
        req.episodes.push_back({e.value("url", ""), e.value("name", ""), e.value("number", -1.0)});
    req.index = index;
    req.oldestFirst = oldestFirst;
    req.forcedToken = forcedToken;
    const auto& ep = dataSource->items[index];
    if (startAt >= 0)
        req.startAt = startAt;
    else if (!ep.value("watched", false))
        req.startAt = ep.value("position", 0.0);
    brls::Application::pushActivity(new PlayerActivity(req), brls::TransitionAnimation::NONE);
}

void AnimeActivity::chooseVideo(int index) {
    const auto& ep = dataSource->items[index];
    auto sid = sourceId;
    std::string epUrl = ep.value("url", ""), epName = ep.value("name", "");
    brls::Application::notify(tr("Cerco i video..."));
    runAsync<json>(
        alive, [sid, epUrl, epName] { return api::hosters(sid, epUrl, epName); },
        [this, index, sid, epUrl](json hs) {
            struct Opt {
                std::string label;
                std::string token;
                int lazyIndex;
            };
            auto opts = std::make_shared<std::vector<Opt>>();
            for (auto& h : hs.value("hosters", json::array())) {
                std::string hn = h.value("name", "");
                if (h["videos"].is_array()) {
                    for (auto& v : h["videos"]) opts->push_back({hn + " - " + v.value("title", ""), v.value("token", ""), -1});
                    if (h["videos"].empty() && h["error"].is_string())
                        opts->push_back({tr("{} (errore)", hn), "", -2});
                } else {
                    opts->push_back({tr("{} (carica...)", hn), "", h.value("index", 0)});
                }
            }
            if (opts->empty()) {
                brls::Application::notify(tr("Nessun video disponibile"));
                return;
            }
            std::vector<std::string> labels;
            for (auto& o : *opts) labels.push_back(o.label);
            // l'azione va eseguita nel dismissCb: il cb normale viene chiamato prima che il menu si chiuda
            auto* dd = new brls::Dropdown(tr("Scegli il video"), labels, [](int) {}, 0, [this, opts, index, sid, epUrl](int sel) {
                if (sel < 0 || sel >= (int)opts->size()) return;
                auto o = (*opts)[sel];
                if (o.lazyIndex == -2) return;
                if (o.lazyIndex < 0) {
                    playEpisode(index, o.token);
                    return;
                }
                runAsync<json>(
                    alive, [sid, epUrl, o] { return api::hosterVideos(sid, epUrl, o.lazyIndex); },
                    [this, index](json r) {
                        auto vids = r.value("videos", json::array());
                        if (vids.empty()) {
                            brls::Application::notify(tr("Nessun video in questo hoster"));
                            return;
                        }
                        std::vector<std::string> l2;
                        for (auto& v : vids) l2.push_back(v.value("title", tr("Video")));
                        auto* d2 = new brls::Dropdown(tr("Qualita'"), l2, [](int) {}, 0, [this, index, vids](int s) {
                            if (s >= 0 && s < (int)vids.size()) playEpisode(index, vids[s].value("token", ""));
                        });
                        brls::Application::pushActivity(new brls::Activity(d2));
                    });
            });
            brls::Application::pushActivity(new brls::Activity(dd));
        });
}

void AnimeActivity::downloadEpisode(int index) {
    if (!dataSource || index < 0 || index >= (int)dataSource->items.size()) return;
    const auto& ep = dataSource->items[index];
    std::string epUrl = ep.value("url", "");
    std::string st = downloads::status(sourceId, epUrl);
    if (st == "done") {
        brls::Application::notify(tr("Gia' scaricato: lo trovi in \"Scaricati\""));
        return;
    }
    if (st == "queued" || st == "downloading") {
        brls::Application::notify(tr("Gia' in coda di download"));
        return;
    }
    downloads::EpisodeInfo info;
    info.sourceId = sourceId;
    info.animeUrl = url;
    info.animeTitle = title;
    info.thumbnail = thumbnail;
    info.episodeUrl = epUrl;
    info.episodeName = ep.value("name", "");
    info.number = ep.value("number", -1.0);
    downloads::enqueue(info);
    brls::Application::notify(downloads::paused() ? tr("Aggiunto alla coda (i download sono in pausa)")
                                                  : tr("Aggiunto alla coda: {}", info.episodeName));
}

void AnimeActivity::openSeason(int index) {
    const auto& s = dataSource->items[index];
    brls::Application::pushActivity(
        new AnimeActivity(sourceId, s.value("url", ""), s.value("title", ""),
                          s["thumbnail"].is_string() ? s["thumbnail"].get<std::string>() : ""));
}
