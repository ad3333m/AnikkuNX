#include "app/meta.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "net/http.hpp"

using json = nlohmann::json;

namespace meta {

namespace {

std::mutex cacheMutex;
std::map<std::string, std::shared_ptr<const Info>> cache;

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '-' || s.back() == ':')) s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

std::string str(const json& j, const char* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}

std::string stripHtml(const std::string& s) {
    std::string out;
    bool tag = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '<') {
            if (s.compare(i, 3, "<br") == 0) out += ' ';
            tag = true;
        } else if (s[i] == '>') {
            tag = false;
        } else if (!tag) {
            out += s[i];
        }
    }
    // AniList appends "(Source: ...)" / "Note: ..." credits
    for (const char* marker : {"(Source:", "Source:", "Note:", "(Note:"}) {
        auto p = out.find(marker);
        if (p != std::string::npos && p > 40) out = out.substr(0, p);
    }
    return trim(out);
}

std::string imageOf(const json& images, const char* type) {
    if (!images.is_array()) return "";
    for (auto& i : images)
        if (str(i, "coverType") == type) return str(i, "url");
    return "";
}

}  // namespace

std::string searchTitle(const std::string& title) {
    std::string s;
    int depth = 0;
    for (char c : title) {  // drop "(Dub)", "[Uncensored]"...
        if (c == '(' || c == '[') depth++;
        else if ((c == ')' || c == ']') && depth > 0) depth--;
        else if (depth == 0) s += c;
    }
    s = trim(s);
    std::string l = lower(s);
    for (const char* suffix : {" english dubbed", " english subbed", " dubbed", " subbed", " dub", " sub", " sub ita",
                               " ita", " (tv)", " tv"}) {
        size_t n = strlen(suffix);
        if (l.size() > n && l.compare(l.size() - n, n, suffix) == 0) {
            s = trim(s.substr(0, s.size() - n));
            l = lower(s);
        }
    }
    return trim(s);
}

std::shared_ptr<const Info> lookup(const std::string& rawTitle) {
    std::string q = searchTitle(rawTitle);
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = cache.find(q);
        if (it != cache.end()) return it->second;
    }
    auto info = std::make_shared<Info>();
    bool networkError = false;
    try {
        json req = {{"query",
                     "query($s:String){Media(search:$s,type:ANIME){id bannerImage averageScore genres "
                     "description(asHtml:false) title{english romaji}}}"},
                    {"variables", {{"s", q}}}};
        auto r = http::request("POST", "https://graphql.anilist.co",
                               {{"Content-Type", "application/json"}, {"Accept", "application/json"}}, req.dump(), 15);
        json m = json::parse(r.body, nullptr, false);
        if (!m.is_discarded() && m["data"]["Media"].is_object()) {
            json media = m["data"]["Media"];
            info->found = true;
            info->anilistId = media.value("id", 0);
            info->banner = str(media, "bannerImage");
            info->score = media["averageScore"].is_number() ? media["averageScore"].get<int>() : 0;
            info->description = stripHtml(str(media, "description"));
            info->englishTitle = str(media["title"], "english");
            if (info->englishTitle.empty()) info->englishTitle = str(media["title"], "romaji");
            if (media["genres"].is_array())
                for (auto& g : media["genres"])
                    if (g.is_string()) info->genres.push_back(g.get<std::string>());
        }
        if (info->anilistId > 0) {
            std::string body =
                http::getText("https://api.ani.zip/mappings?anilist_id=" + std::to_string(info->anilistId), {}, 15);
            json z = json::parse(body, nullptr, false);
            if (!z.is_discarded() && z.is_object()) {
                info->fanart = imageOf(z["images"], "Fanart");
                info->logo = imageOf(z["images"], "Clearlogo");
                if (z["episodes"].is_object()) {
                    for (auto& kv : z["episodes"].items()) {
                        json e = kv.value();
                        int n = std::atoi(kv.key().c_str());
                        if (n <= 0) continue;
                        Episode ep;
                        if (e["title"].is_object()) ep.title = str(e["title"], "en");
                        ep.image = str(e, "image");
                        std::string rt = e["runtime"].is_string() ? e["runtime"].get<std::string>() : "";
                        ep.runtime = e["runtime"].is_number() ? e["runtime"].get<int>() : std::atoi(rt.c_str());
                        info->episodes[n] = ep;
                    }
                }
            }
        }
    } catch (const std::exception&) {
        // offline or rate limited: fall back to the source's own cover and text, retry next time
        networkError = true;
    }
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (!networkError || info->found) cache[q] = info;
    return info;
}

}  // namespace meta
