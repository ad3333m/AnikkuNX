#pragma once

#include <borealis.hpp>
#include <borealis/views/h_scrolling_frame.hpp>

#include <functional>
#include <string>
#include <vector>

#include "view/anime_grid.hpp"
#include "view/cover_image.hpp"

namespace cr {

inline NVGcolor orange() { return nvgRGB(244, 117, 33); }
inline NVGcolor bar() { return nvgRGB(35, 37, 43); }
inline NVGcolor text() { return nvgRGB(255, 255, 255); }
inline NVGcolor muted() { return nvgRGB(160, 160, 166); }
inline NVGcolor dim() { return nvgRGB(110, 110, 118); }

std::string icon(unsigned cp);
const unsigned ICON_SEARCH = 0xE8B6, ICON_SETTINGS = 0xE8B8, ICON_BOOKMARK = 0xE866, ICON_BOOKMARK_BORDER = 0xE867,
               ICON_WARNING = 0xE002, ICON_BACK = 0xE5C4, ICON_DOWNLOAD = 0xE2C4, ICON_DONE = 0xE876,
               ICON_QUEUED = 0xE8B5, ICON_ERROR = 0xE000, ICON_SORT = 0xE164, ICON_CHECK_CIRCLE = 0xE86C;

/** Orange crescent mark in front of the wordmark. */
class LogoMark : public brls::View {
  public:
    LogoMark();
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx) override;
};

/** Solid play triangle for orange call-to-action buttons. */
class PlayGlyph : public brls::View {
  public:
    explicit PlayGlyph(NVGcolor c);
    void draw(NVGcontext* vg, float x, float y, float w, float h, brls::Style style, brls::FrameContext* ctx) override;

  private:
    NVGcolor color;
};

/** Logo + "AnikkuNX" wordmark. */
brls::Box* logo();

/**
 * Black page for secondary screens: a top bar with a back button, the logo and
 * the screen title above `content`. B goes back (or calls onBack when given).
 */
brls::Box* page(const std::string& title, brls::View* content, std::function<void()> onBack = nullptr);

/** Orange (filled) or outlined call-to-action button with an optional leading play glyph. */
class CtaButton : public brls::Box {
  public:
    CtaButton(const std::string& text, bool filled, bool playGlyph, std::function<void()> onClick);
    void setText(const std::string& text);

  private:
    brls::Label* lbl;
};

std::string ellipsize(const std::string& s, size_t maxChars);
/** Breaks text into lines that fit `width` with explicit newlines (borealis keeps a leading space on wrapped lines). */
std::string wrap(const std::string& s, float fontSize, float width, int maxLines = 0);

/** Label drawn twice with a sub-pixel offset: the console font has no bold weight. */
class BoldLabel : public brls::Label {
  public:
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
};

brls::Label* label(const std::string& text, float size, NVGcolor color, bool bold = false);

/** Material icon glyph of a fixed size. */
class IconView : public brls::View {
  public:
    IconView(unsigned codepoint, float size, NVGcolor color);
    void setIcon(unsigned codepoint) { glyph = icon(codepoint); }
    void setColor(NVGcolor c) { color = c; }
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    std::string glyph;
    float size;
    NVGcolor color;
};

/** Text link in the top bar: grey, white with an orange underline when focused. */
class NavItem : public brls::Box {
  public:
    NavItem(const std::string& text, std::function<void()> onClick);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    brls::Label* lbl;
};

/** Round icon button in the top bar (search, settings). */
class NavIcon : public brls::Box {
  public:
    NavIcon(unsigned codepoint, std::function<void()> onClick);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    IconView* iconView;
};

/** Portrait poster with title and grey subtitle underneath; optional "+N" badge and watch progress bar. */
class PosterCard : public brls::Box {
  public:
    PosterCard(const GridItem& item, float width);
    GridItem item;
    /** 0..1, drawn as an orange bar along the bottom of the cover (Continue Watching). */
    float progress = -1;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    float coverW, coverH;
};

/** A titled horizontal shelf of posters, ending in a "See all" tile when onSeeAll is set. */
class PosterRow : public brls::Box {
  public:
    explicit PosterRow(const std::string& title, const std::string& subtitle = "");
    void setItems(const std::vector<GridItem>& items);

    std::function<void(const GridItem&)> onSelect;
    std::function<void()> onSeeAll;
    /** Optional: + on a poster opens an options menu. */
    std::function<void(const GridItem&)> onOptions;
    /** Optional: how far into the episode an item is (Continue Watching). */
    std::function<float(const GridItem&)> progressOf;

  private:
    brls::HScrollingFrame* scroller;
    brls::Box* strip;
};

/** Full-bleed backdrop for the hero: the cover zoomed, softly blurred and faded into black. */
class HeroBackdrop : public CoverImage {
  public:
    HeroBackdrop();
    /** Alpha (0-255) of the black layer over the whole image. */
    float dim = 90;
    /** Soften the image (for upscaled portrait covers); off for real wide fanart. */
    bool blur = true;
    /** Where the image is cropped vertically when it is taller than the view (0 top, 1 bottom). */
    float focusY = 0.22f;
    /** Width fraction covered by the left fade to black. */
    float leftFade = 0.72f;
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;
};

/** Carousel position dots: the active one is a longer orange pill. */
class PageDots : public brls::View {
  public:
    PageDots();
    void setState(int count, int active);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    int count = 0, active = 0;
};

/** Big chevron drawn at the hero's left/right edge; tapping it changes slide. */
class HeroArrow : public brls::View {
  public:
    HeroArrow(bool right, std::function<void()> onTap);
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

  private:
    bool right;
};

}  // namespace cr
