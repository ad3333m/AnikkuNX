#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * Artwork and English metadata for a show, looked up by title on AniList
 * (banner, synopsis, genres, score) and ani.zip (TVDB fanart, clear logo,
 * per-episode thumbnails, titles and runtimes). The anime sites themselves
 * only provide a cover, so this is what makes the pages look like a
 * streaming service.
 */
namespace meta {

struct Episode {
    std::string title;  // English
    std::string image;  // 16:9 still
    int runtime = 0;    // minutes
};

struct Info {
    bool found = false;
    int anilistId = 0;
    std::string englishTitle;
    std::string fanart;  // wide background art
    std::string logo;    // transparent PNG title logo
    std::string banner;  // AniList banner (fallback for fanart)
    std::string description;
    std::vector<std::string> genres;
    int score = 0;  // 0-100
    std::map<int, Episode> episodes;  // by episode number
};

/** Blocking (use runAsync). Results, including misses, are cached for the session. */
std::shared_ptr<const Info> lookup(const std::string& title);

/** "Bleach (Dub)" -> "Bleach": strips the tags anime sites add to titles. */
std::string searchTitle(const std::string& title);

}  // namespace meta
