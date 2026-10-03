#pragma once

#include <borealis.hpp>
#include <borealis/views/h_scrolling_frame.hpp>

#include <functional>
#include <string>
#include <vector>

#include "view/anime_grid.hpp"

/**
 * Netflix/Crunchyroll-style horizontal row: a title header on top and a
 * horizontally scrolling strip of AnimeCard covers under it. Used by the
 * Discover tab to compose a vertical stack of themed rows.
 *
 * When the user focuses the trailing "See all" chip (or clicks it) the
 * onSeeAll callback is invoked — typically pushing the full BrowseActivity
 * for that source.
 */
class AnimeRow : public brls::Box {
  public:
    explicit AnimeRow(const std::string& title, float cardWidth = 180.f);

    /** Replace the row items. Empty vector shows the status placeholder. */
    void setItems(const std::vector<GridItem>& items);
    void showLoading();
    void showMessage(const std::string& text);

    /** Change the subtitle shown under the title (optional, muted color). */
    void setSubtitle(const std::string& text);
    /** Make the whole row disappear (used by the "Continue watching" row when empty). */
    void setHidden(bool hidden);

    std::function<void(const GridItem&)> onSelect;
    std::function<void()> onSeeAll;

  private:
    brls::Label* titleLabel;
    brls::Label* subtitleLabel;
    brls::Label* statusLabel;
    brls::HScrollingFrame* scroller;
    brls::Box* strip;
    float cardWidth;
};
