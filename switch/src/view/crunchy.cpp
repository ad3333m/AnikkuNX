#include "view/crunchy.hpp"

#include <algorithm>
#include <cmath>

namespace cr {

static const float CARD_W = 176;

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

// ---------------------------------------------------------------------------- top bar

NavItem::NavItem(const std::string& text, std::function<void()> onClick) : brls::Box(brls::Axis::ROW) {
    setFocusable(true);
    setHideHighlight(true);
    setHeight(64);
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
    setHeight(64);
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

    addGestureRecognizer(new brls::TapGestureRecognizer(this));
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
    t->setMarginLeft(60);
    addView(t);
    if (!subtitle.empty()) {
        auto* s = label(subtitle, 15, muted());
        s->setMarginLeft(60);
        s->setMarginTop(4);
        addView(s);
    }

    strip = new brls::Box(brls::Axis::ROW);
    strip->setAlignItems(brls::AlignItems::FLEX_START);
    strip->setPadding(14, 60, 10, 60);

    scroller = new brls::HScrollingFrame();
    scroller->setHeight(std::round(CARD_W * 1.5f) + 84);
    scroller->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
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
        float s = std::max(w / iw, h / ih) * 1.05f;
        float dw = iw * s, dh = ih * s;
        float ox = x + (w - dw) / 2, oy = y + (h - dh) * 0.22f;
        // cheap blur: average a ring of offset copies (each pass blends 1/k on top of the previous ones)
        const int N = 9;
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
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 90));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x, y, w * 0.72f, h);
    nvgFillPaint(vg, nvgLinearGradient(vg, x, y, x + w * 0.72f, y, nvgRGBA(0, 0, 0, 245), nvgRGBA(0, 0, 0, 0)));
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
