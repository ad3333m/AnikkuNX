#include "view/anime_row.hpp"
#include "util/i18n.hpp"

namespace {

/** Trailing chip card: a dark rounded button at the end of the row that opens the full source list. */
class SeeAllCard : public brls::Box {
  public:
    SeeAllCard(float width, float height, std::function<void()> onClick) : brls::Box(brls::Axis::COLUMN) {
        setWidth(width);
        setHeight(height);
        setMargins(0, 8, 0, 8);
        setFocusable(true);
        setCornerRadius(10);
        setHighlightCornerRadius(12);
        setBackgroundColor(nvgRGBA(255, 255, 255, 22));
        setJustifyContent(brls::JustifyContent::CENTER);
        setAlignItems(brls::AlignItems::CENTER);

        auto* arrow = new brls::Label();
        arrow->setText("›");  // small right-pointing chevron
        arrow->setFontSize(42);
        arrow->setTextColor(nvgRGB(230, 230, 240));
        addView(arrow);

        auto* lbl = new brls::Label();
        lbl->setText(tr("Vedi tutto"));
        lbl->setFontSize(15);
        lbl->setTextColor(nvgRGB(200, 200, 210));
        lbl->setMarginTop(6);
        addView(lbl);

        registerClickAction([onClick](brls::View*) {
            if (onClick) onClick();
            return true;
        });
        addGestureRecognizer(new brls::TapGestureRecognizer(this));
    }
};

}  // namespace

AnimeRow::AnimeRow(const std::string& title, float cw) : brls::Box(brls::Axis::COLUMN), cardWidth(cw) {
    setMargins(10, 0, 20, 0);
    setAlignItems(brls::AlignItems::FLEX_START);

    titleLabel = new brls::Label();
    titleLabel->setText(title);
    titleLabel->setFontSize(22);
    titleLabel->setTextColor(nvgRGB(235, 235, 240));
    titleLabel->setMarginBottom(2);
    addView(titleLabel);

    subtitleLabel = new brls::Label();
    subtitleLabel->setText("");
    subtitleLabel->setFontSize(14);
    subtitleLabel->setTextColor(nvgRGB(140, 140, 150));
    subtitleLabel->setMarginBottom(6);
    subtitleLabel->setVisibility(brls::Visibility::GONE);
    addView(subtitleLabel);

    statusLabel = new brls::Label();
    statusLabel->setText("");
    statusLabel->setFontSize(16);
    statusLabel->setTextColor(nvgRGB(150, 150, 160));
    statusLabel->setMargins(10, 0, 10, 0);
    statusLabel->setVisibility(brls::Visibility::GONE);
    addView(statusLabel);

    strip = new brls::Box(brls::Axis::ROW);
    strip->setAlignItems(brls::AlignItems::FLEX_START);
    strip->setPaddingLeft(0);
    strip->setPaddingRight(30);

    scroller = new brls::HScrollingFrame();
    scroller->setContentView(strip);
    scroller->setHeight(cardWidth * 1.42f + 110);  // cover + title band + card padding/margin
    scroller->setGrow(1);
    scroller->setScrollingBehavior(brls::ScrollingBehavior::NATURAL);
    addView(scroller);
}

void AnimeRow::setItems(const std::vector<GridItem>& items) {
    statusLabel->setVisibility(brls::Visibility::GONE);
    strip->clearViews();
    for (const auto& it : items) {
        auto* card = new AnimeCard(it, cardWidth);
        card->registerClickAction([this, card](brls::View*) {
            if (onSelect) onSelect(card->item);
            return true;
        });
        card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
        strip->addView(card);
    }
    if (onSeeAll) {
        float h = cardWidth * 1.42f;
        strip->addView(new SeeAllCard(cardWidth * 0.55f, h, onSeeAll));
    }
    scroller->setVisibility(brls::Visibility::VISIBLE);
}

void AnimeRow::showLoading() { showMessage(tr("Caricamento...")); }

void AnimeRow::showMessage(const std::string& text) {
    strip->clearViews();
    statusLabel->setText(text);
    statusLabel->setVisibility(brls::Visibility::VISIBLE);
    scroller->setVisibility(brls::Visibility::GONE);
}

void AnimeRow::setSubtitle(const std::string& text) {
    if (text.empty()) {
        subtitleLabel->setVisibility(brls::Visibility::GONE);
    } else {
        subtitleLabel->setText(text);
        subtitleLabel->setVisibility(brls::Visibility::VISIBLE);
    }
}

void AnimeRow::setHidden(bool hidden) {
    setVisibility(hidden ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
}
