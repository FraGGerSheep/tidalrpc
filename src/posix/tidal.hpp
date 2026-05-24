#pragma once
#include "../json_min.hpp"
#include "http.hpp"
#include <string>
#include <cctype>

namespace tidal {

struct Info {
    std::string cover;
    std::string tidalUrl;
    std::string trackId;
};

inline std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            o += static_cast<char>(c);
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 0xF];
        }
    }
    return o;
}

inline Info lookup(const std::string& title, const std::string& artist) {
    Info info;

    std::string body = http::get(
        "https://api.tidal.com/v1/search/tracks?query="
        + urlEncode(title + " " + artist)
        + "&limit=3&countryCode=US&token=CzET4vdadNUFQ5JU");

    std::string cover = json::findString(body, "cover");
    if (!cover.empty()) {
        std::string p;
        for (char c : cover) p += (c == '-') ? '/' : c;
        info.cover = "https://resources.tidal.com/images/" + p + "/640x640.jpg";
    }

    std::string url = json::findString(body, "url");
    if (!url.empty()) {
        if (url.rfind("http://", 0) == 0) url = "https://" + url.substr(7);
        info.tidalUrl = url;
        size_t s = url.find_last_of('/');
        if (s != std::string::npos) {
            std::string id = url.substr(s + 1);
            bool digits = !id.empty();
            for (char c : id)
                if (!std::isdigit((unsigned char)c)) { digits = false; break; }
            if (digits) info.trackId = id;
        }
    }

    if (info.cover.empty()) {
        std::string ib = http::get(
            "https://itunes.apple.com/search?term="
            + urlEncode(artist + " " + title) + "&entity=song&limit=1");
        std::string art = json::findString(ib, "artworkUrl100");
        if (!art.empty()) {
            size_t pos = art.find("100x100");
            if (pos != std::string::npos) art.replace(pos, 7, "600x600");
            info.cover = art;
        }
    }
    return info;
}

}
