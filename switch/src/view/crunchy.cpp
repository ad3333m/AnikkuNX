#include "view/crunchy.hpp"

#include <algorithm>
#include <cmath>

namespace cr {

static const float CARD_W = 176;

static float aspect() {
    float h = (float)brls::Application::contentHeight;
    return h > 0 ? (float)brls::Application::contentWidth / h : 16.0f / 9.0f;
}

bool isPhone() { return aspect() > 1.95f; }
bool isTablet() { return aspect() < 1.5f; }
float sideInset() { return isPhone() ? 64.0f : 0.0f; }
float topBarHeight() { return isPhone() ? 56.0f : 64.0f; }

std::string optionsHint(const std::string& where) {
#ifdef IOS
    return "Tap â® " + where + " for more options";
#else
    return "Press + " + where + " for more options";
#endif
}

void drawOptionsDots(NVGcontext* vg, float cx, float cy, float r, bool circle) {
    if (circle) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 170));
        nvgFill(vg);
    }
    float d = r * 0.42f, dot = std::max(1.6f, r * 0.13f);
    for (int i = -1; i <= 1; i++) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy + i * d, dot);
        nvgFillColor(vg, nvgRGB(240, 240, 245));
        nvgFill(vg);
    }
}

std::string icon(unsigned cp) {
    std::string s;
    s += (char)(0xE0 | (cp >> 12));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
    return s;
}

std::string ellipsize(const std::string& s, size_t maxChars) {
    if (s.size() <= maxChars) return s;
    size_t cut = maxChars;
    while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) cut--;  // stay on a UTF-8 boundary
    std::string out = s.substr(0, cut);
    while (!out.empty() && (out.back() == ' ' || out.back() == ',' || out.back() == '.')) out.pop_back();
    return out + "\xE2\x80\xA6";
}

std::string wrap(const std::string& s, float fontSize, float width, int maxLines) {
    NVGcontext* vg = brls::Application::getNVGContext();
    if (!vg || s.empty()) return s;
    nvgSave(vg);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
    nvgFontSize(vg, fontSize);
    nvgTextLetterSpacing(vg, 0);
    std::vector<std::string> lines;
    const char* p = s.c_str();
    const char* end = p + s.size();
    NVGtextRow rows[8];
    int n;
    bool truncated = false;
    while (p < end && (n = nvgTextBreakLines(vg, p, end, width, rows, 8)) > 0) {
        for (int i = 0; i < n; i++) {
            std::string line(rows[i].start, rows[i].end);
            size_t a = line.find_first_not_of(' ');
            if (a == std::string::npos) continue;
            if (maxLines > 0 && (int)lines.size() == maxLines) {
                truncated = true;
                break;
            }
            lines.push_back(line.substr(a));
        }
        if (truncated) break;
        p = rows[n - 1].next;
    }
    if (truncated && !lines.empty()) {
        // shorten the last line until it fits with an ellipsis
        std::string& last = lines.back();
        float b[4];
        while (!last.empty()) {
            std::string t = last + "\xE2\x80\xA6";
            if (nvgTextBounds(vg, 0, 0, t.c_str(), nullptr, b) <= width) break;
            last.pop_back();
            while (!last.empty() && ((unsigned char)last.back() & 0xC0) == 0x80) last.pop_back();
            if (!last.empty() && ((unsigned char)last.back() & 0xC0) == 0xC0) last.pop_back();
        }
        while (!last.empty() && last.back() == ' ') last.pop_back();
        last += "\xE2\x80\xA6";
    }
    nvgRestore(vg);
    std::string out;
    for (auto& l : lines) out += (out.empty() ? "" : "\n") + l;
    return out;
}

void BoldLabel::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                     brls::FrameContext* ctx) {
    brls::Label::draw(vg, x, y, width, height, style, ctx);
    brls::Label::draw(vg, x + 0.7f, y, width, height, style, ctx);
}

brls::Label* label(const std::string& text, float size, NVGcolor color, bool bold) {
    brls::Label* l = bold ? new BoldLabel() : new brls::Label();
    l->setText(text);
    l->setFontSize(size);
    l->setTextColor(color);
    return l;
}

// ---------------------------------------------------------------------------- IconView

IconView::IconView(unsigned cp, float sz, NVGcolor c) : glyph(icon(cp)), size(sz), color(c) {
    setWidth(sz);
    setHeight(sz);
}

void IconView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*) {
    int f = brls::Application::getFont(brls::FONT_MATERIAL_ICONS);
    if (f < 0) return;
    nvgFontFaceId(vg, f);
    nvgFontSize(vg, size);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + width / 2, y + height / 2, glyph.c_str(), nullptr);
    nvgFontFaceId(vg, brls::Application::getDefaultFont());
}

// ---------------------------------------------------------------------------- chrome

LogoMark::LogoMark() {
    setWidth(34);
    setHeight(34);
}

void LogoMark::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) {
    float cx = x + w / 2, cy = y + h / 2;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 15);
    nvgFillColor(vg, orange());
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx + 4.5f, cy - 2.5f, 8.5f);
    nvgFillColor(vg, bar());
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx + 6.5f, cy - 4.5f, 3.5f);
    nvgFillColor(vg, orange());
    nvgFill(vg);
}

PlayGlyph::PlayGlyph(NVGcolor c) : color(c) {
    setWidth(15);
    setHeight(18);
}

void PlayGlyph::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + 1, y + 1);
    nvgLineTo(vg, x + w - 1, y + h / 2);
    nvgLineTo(vg, x + 1, y + h - 1);
    nvgClosePath(vg);
    nvgFillColor(vg, color);
    nvgFill(vg);
}

brls::Box* logo() {
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->addView(new LogoMark());
    auto* word = label("AnikkuNX", 24, orange(), true);
    word->setMarginLeft(8);
    box->addView(word);
    return box;
}

namespace {

/** Page root: focus starts in the content, not on the back button. */
class Page : public brls::Box {
  public:
    explicit Page(brls::View* c) : brls::Box(brls::Axis::COLUMN), content(c) {}
    brls::View* getDefaultFocus() override {
        brls::View* f = content->getDefaultFocus();
        return f ? f : brls::Box::getDefaultFocus();
    }

  private:
    brls::View* content;
};

}  // namespace

brls::Box* page(const std::string& title, brls::View* content, std::function<void()> onBack, bool fullBleed) {
    std::function<void()> back = onBack ? onBack : [] { brls::Application::popActivity(); };
    auto* root = new Page(content);
    root->setGrow(1);
    root->setBackgroundColor(nvgRGB(0, 0, 0));

    auto* topBar = new brls::Box(brls::Axis::ROW);
    topBar->setHeight(topBarHeight());
    topBar->setBackgroundColor(bar());
    topBar->setAlignItems(brls::AlignItems::CENTER);
    topBar->setPadding(0, 36 + sideInset(), 0, 12 + sideInset());
    topBar->addView(new NavIcon(ICON_BACK, back));
    auto* lg = logo();
    lg->setMarginLeft(10);
    topBar->addView(lg);
    if (!title.empty()) {
        auto* sep = new brls::Box(brls::Axis::ROW);
        sep->setWidth(2);
        sep->setHeight(28);
        sep->setMargins(0, 22, 0, 22);
        sep->setBackgroundColor(nvgRGB(70, 70, 78));
        topBar->addView(sep);
        topBar->addView(label(title, 22, text(), true));
    }
    root->addView(topBar);

    content->setGrow(1);
    if (fullBleed || sideInset() <= 0) {
        root->addView(content);
    } else {
        auto* body = new brls::Box(brls::Axis::COLUMN);
        body->setGrow(1);
        body->setPadding(0, sideInset(), 0, sideInset());
        body->addView(content);
        root->addView(body);
    }
    root->registerAction(
        "Back", brls::BUTTON_B,
        [back](brls::View*) {
            back();
            return true;
        },
        false, false, brls::SOUND_BACK);
    return root;
}

CtaButton::CtaButton(const std::string& text, bool filled, bool playGlyph, std::function<void()> onClick)
    : brls::Box(brls::Axis::ROW) {
    setFocusable(true);
    setHeight(46);
    setPadding(0, 22, 0, playGlyph ? 18 : 22);
    setAlignItems(brls::AlignItems::CENTER);
    setJustifyContent(brls::JustifyContent::CENTER);
    setHideHighlightBackground(true);
    setHighlightCornerRadius(2);
    setHighlightPadding(4);
    NVGcolor fg = filled ? nvgRGB(0, 0, 0) : orange();
    if (filled) {
        setBackgroundColor(orange());
    } else {
        setBorderColor(orange());
        setBorderThickness(2);
    }
    setShrink(0);
    if (playGlyph) addView(new PlayGlyph(fg));
    lbl = label(text, 16, fg, true);
    lbl->setSingleLine(true);
    if (playGlyph) lbl->setMarginLeft(10);
    addView(lbl);
    registerClickAction([onClick](brls::View*) {
        onClick();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void CtaButton::setText(const std::string& text) { lbl->setText(text); }

// ---------------------------------------------------------------------------- top bar

NavItem::NavItem(const std::string& text, std::function<void()> onClick) : brls::Box(brls::Axis::ROW) {
    setFocusable(true);
    setHideHighlight(true);
    setHeight(topBarHeight());
    setPadding(0, 16, 0, 16);
    setAlignItems(brls::AlignItems::CENTER);
    lbl = label(text, 17, nvgRGB(218, 218, 222));
    addView(lbl);
    registerClickAction([onClick](brls::View*) {
        onClick();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void NavItem::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                   brls::FrameContext* ctx) {
    bool f = isFocused();
    if (f) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGB(20, 21, 25));
        nvgFill(vg);
    }
    lbl->setTextColor(f ? text() : nvgRGB(218, 218, 222));
    brls::Box::draw(vg, x, y, width, height, style, ctx);
    if (f) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y + height - 4, width, 4);
        nvgFillColor(vg, orange());
        nvgFill(vg);
    }
}

NavIcon::NavIcon(unsigned cp, std::function<void()> onClick) : brls::Box(brls::Axis::ROW) {
    setFocusable(true);
    setHideHighlight(true);
    setWidth(56);
    setHeight(topBarHeight());
    setJustifyContent(brls::JustifyContent::CENTER);
    setAlignItems(brls::AlignItems::CENTER);
    iconView = new IconView(cp, 28, nvgRGB(218, 218, 222));
    addView(iconView);
    registerClickAction([onClick](brls::View*) {
        onClick();
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

void NavIcon::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                   brls::FrameContext* ctx) {
    bool f = isFocused();
    if (f) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, width, height);
        nvgFillColor(vg, nvgRGB(20, 21, 25));
        nvgFill(vg);
    }
    iconView->setColor(f ? text() : nvgRGB(218, 218, 222));
    brls::Box::draw(vg, x, y, width, height, style, ctx);
    if (f) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y + height - 4, width, 4);
        nvgFillColor(vg, orange());
        nvgFill(vg);
    }
}

// ---------------------------------------------------------------------------- PosterCard

PosterCard::PosterCard(const GridItem& it, float width) : brls::Box(brls::Axis::COLUMN), item(it) {
    coverW = width;
    coverH = std::round(width * 1.5f);
    setWidth(width);
    setMarginRight(18);
    setFocusable(true);
    setHideHighlightBackground(true);
    setHighlightCornerRadius(4);
    setHighlightPadding(5);

    auto* img = new CoverImage();
    img->setWidth(coverW);
    img->setHeight(coverH);
    img->setCornerRadius(2);
    img->setBackgroundColor(nvgRGB(35, 37, 43));
    addView(img);
    img->setUrl(item.thumbnail);

    auto* title = label(ellipsize(item.title, 60), 15, text(), true);
    title->setSingleLine(true);
    title->setWidth(coverW);
    title->setMarginTop(10);
    addView(title);

    auto* sub = label(item.subtitle.empty() ? " " : item.subtitle, 13, muted());
    sub->setSingleLine(true);
    sub->setWidth(coverW);
    sub->setMarginTop(4);
    addView(sub);

    // tap: the ⋮ on the cover opens the options, anywhere else opens the show
    addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus st, brls::Sound* snd) {
        if (st.state != brls::GestureState::END) return;
        *snd = brls::SOUND_CLICK;
        auto f = getFrame();
        float cx = f.getMinX() + coverW - 22, cy = f.getMinY() + 22;
        float dx = st.position.x - cx, dy = st.position.y - cy;
        brls::Application::giveFocus(this);
        if (onOptions && dx * dx + dy * dy <= 34 * 34)
            onOptions();
        else if (onActivate)
            onActivate();
    }));
}

void PosterCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext* ctx) {
    brls::Box::draw(vg, x, y, width, height, style, ctx);
    if (!item.badge.empty()) {
        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgFontSize(vg, 15);
        float b[4];
        nvgTextBounds(vg, 0, 0, item.badge.c_str(), nullptr, b);
        float w = b[2] - b[0] + 14, h = 24, bx = x + 8, by = y + 8;
        nvgBeginPath(vg);
        nvgRect(vg, bx, by, w, h);
        nvgFillColor(vg, orange());
        nvgFill(vg);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, bx + w / 2, by + h / 2 + 1, item.badge.c_str(), nullptr);
    }
    if (onOptions) drawOptionsDots(vg, x + coverW - 22, y + 22, 16, true);
    if (progress >= 0) {
        float p = std::min(1.0f, std::max(0.0f, progress));
        nvgBeginPath(vg);
        nvgRect(vg, x, y + coverH - 5, coverW, 5);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 70));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, x, y + coverH - 5, coverW * p, 5);
        nvgFillColor(vg, orange());
        nvgFill(vg);
    }
}

// ---------------------------------------------------------------------------- PosterRow

namespace {

class SeeAllTile : public brls::Box {
  public:
    SeeAllTile(float h, std::function<void()> onClick) : brls::Box(brls::Axis::COLUMN) {
        setWidth(150);
        setHeight(h);
        setFocusable(true);
        setHideHighlightBackground(true);
        setHighlightCornerRadius(4);
        setHighlightPadding(5);
        setBackgroundColor(nvgRGB(35, 37, 43));
        setJustifyContent(brls::JustifyContent::CENTER);
        setAlignItems(brls::AlignItems::CENTER);
        addView(new IconView(0xE5CC, 54, orange()));  // chevron_right
        auto* l = label("SEE ALL", 15, text(), true);
        l->setMarginTop(6);
        addView(l);
        registerClickAction([onClick](brls::View*) {
            onClick();
            return true;
        });
        addGestureRecognizer(new brls::TapGestureRecognizer(this));
    }
};

}  // namespace

PosterRow::PosterRow(const std::string& title, const std::string& subtitle) : brls::Box(brls::Axis::COLUMN) {
    setMarginBottom(26);
    setAlignItems(brls::AlignItems::STRETCH);

    auto* t = label(title, 25, text(), true);
    t->setMarginLeft(60 + sideInset());
    addView(t);
    if (!subtitle.empty()) {
        auto* s = label(subtitle, 15, muted());
        s->setMarginLeft(60 + sideInset());
        s->setMarginTop(4);
        addView(s);
    }

    strip = new brls::Box(brls::Axis::ROW);
    strip->setAlignItems(brls::AlignItems::FLEX_START);
    strip->setPadding(14, 60 + sideInset(), 10, 60 + sideInset());

    scroller = new brls::HScrollingFrame();
    scroller->setHeight(std::round(CARD_W * 1.5f) + 84);
    scroller->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
    scroller->setScrollingIndicatorVisible(false);
    scroller->setContentView(strip);
    addView(scroller);
}

void PosterRow::setItems(const std::vector<GridItem>& items) {
    // build the new cards before freeing the old ones so focus never points at a deleted view
    std::vector<brls::View*> old = strip->getChildren();
    bool hadFocus = strip->isChildFocused();
    brls::View* first = nullptr;
    for (const auto& it : items) {
        auto* card = new PosterCard(it, CARD_W);
        if (progressOf) card->progress = progressOf(it);
        card->registerClickAction([this, card](brls::View*) {
            if (onSelect) onSelect(card->item);
            return true;
        });
        card->onActivate = [this, card] {
            if (onSelect) onSelect(card->item);
        };
        if (onOptions) {
            card->registerAction("Options", brls::BUTTON_START, [this, card](brls::View*) {
                if (onOptions) onOptions(card->item);
                return true;
            });
            card->onOptions = [this, card] {
                if (onOptions) onOptions(card->item);
            };
        }
        strip->addView(card);
        if (!first) first = card;
    }
    if (onSeeAll) {
        auto* tile = new SeeAllTile(std::round(CARD_W * 1.5f), onSeeAll);
        strip->addView(tile);
        if (!first) first = tile;
    }
    if (hadFocus && first) brls::Application::giveFocus(first);
    for (auto* v : old) strip->removeView(v);
    strip->setLastFocusedView(hadFocus ? first : nullptr);
    scroller->setContentOffsetX(0, false);
}

// ---------------------------------------------------------------------------- hero

HeroBackdrop::HeroBackdrop() {
    setBackgroundColor(nvgRGBA(0, 0, 0, 0));
    setPositionType(brls::PositionType::ABSOLUTE);
    setPositionTop(0);
    setPositionLeft(0);
    setWidthPercentage(100);
    setHeightPercentage(100);
}

void HeroBackdrop::draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style, brls::FrameContext*) {
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);

    int tex = getTexture();
    float iw = getOriginalImageWidth(), ih = getOriginalImageHeight();
    if (tex && iw > 0 && ih > 0) {
        // zoom the cover to fill, biased towards the top where faces usually are
        float s = std::max(w / iw, h / ih) * (blur ? 1.05f : 1.0f);
        float dw = iw * s, dh = ih * s;
        float ox = x + (w - dw) / 2, oy = y + (h - dh) * focusY;
        // cheap blur: average a ring of offset copies (each pass blends 1/k on top of the previous ones)
        const int N = blur ? 9 : 1;
        const float R = 14;
        for (int i = 0; i < N; i++) {
            float a = i == 0 ? 0 : (float)(i - 1) / (N - 1) * 6.2831853f;
            float dx = i == 0 ? 0 : std::cos(a) * R, dy = i == 0 ? 0 : std::sin(a) * R;
            NVGpaint p = nvgImagePattern(vg, ox + dx, oy + dy, dw, dh, 0, tex, 1.0f / (i + 1));
            nvgBeginPath(vg);
            nvgRect(vg, x, y, w, h);
            nvgFillPaint(vg, p);
            nvgFill(vg);
        }
    }

    // overall dim, then a left-side fade for the text and a bottom fade into the page
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, (unsigned char)dim));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x, y, w * leftFade, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w * leftFade, y, nvgRGBA(0, 0, 0, 245), nvgRGBA(0, 0, 0, 0)));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x, y + h * 0.55f, w, h * 0.45f + 1);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y + h * 0.55f, x, y + h, nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 255)));
    nvgFill(vg);
}

PageDots::PageDots() {
    setHeight(8);
    setWidth(300);
}

void PageDots::setState(int c, int a) {
    count = c;
    active = a;
}

void PageDots::draw(NVGcontext* vg, float x, float y, float, float height, brls::Style, brls::FrameContext*) {
    float cx = x;
    for (int i = 0; i < count; i++) {
        float w = i == active ? 38 : 20;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx, y + (height - 6) / 2, w, 6, 3);
        nvgFillColor(vg, i == active ? orange() : nvgRGBA(255, 255, 255, 90));
        nvgFill(vg);
        cx += w + 8;
    }
}

HeroArrow::HeroArrow(bool r, std::function<void()> onTap) : right(r) {
    setPositionType(brls::PositionType::ABSOLUTE);
    setPositionTop(0);
    if (r)
        setPositionRight(0);
    else
        setPositionLeft(0);
    setWidth(56);
    setHeightPercentage(100);
    addGestureRecognizer(new brls::TapGestureRecognizer([onTap](brls::TapGestureStatus st, brls::Sound* snd) {
        if (st.state != brls::GestureState::END) return;
        *snd = brls::SOUND_CLICK;
        onTap();
    }));
}

void HeroArrow::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style, brls::FrameContext*) {
    float cx = x + width / 2, cy = y + height / 2 - 20, d = 11;
    nvgBeginPath(vg);
    if (right) {
        nvgMoveTo(vg, cx - d / 2, cy - d);
        nvgLineTo(vg, cx + d / 2, cy);
        nvgLineTo(vg, cx - d / 2, cy + d);
    } else {
        nvgMoveTo(vg, cx + d / 2, cy - d);
        nvgLineTo(vg, cx - d / 2, cy);
        nvgLineTo(vg, cx + d / 2, cy + d);
    }
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 220));
    nvgStrokeWidth(vg, 3.5f);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    nvgStroke(vg);
}

}  // namespace cr
