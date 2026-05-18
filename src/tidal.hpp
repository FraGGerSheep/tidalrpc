// Cover-Lookup: erst Tidal-Suche, dann iTunes als Fallback.
#pragma once
#include "http.hpp"
#include "json_min.hpp"
#include <windows.h>
#include <string>
#include <cctype>

namespace tidal {

inline std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

inline std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            o += (char)c;
        } else {
            o += '%';
            o += hex[c >> 4];
            o += hex[c & 0xF];
        }
    }
    return o;
}

struct Info {
    std::string cover;    // Bild-URL fuer Discord large_image
    std::string tidalUrl; // https-Track-Link fuer den "Play on Tidal"-Button
    std::string trackId;  // numerische Track-ID fuer tidal://track/<id>
};

// Tidal-Suche: liefert Cover + Track-URL. Token ist der oeffentlich bekannte
// Desktop-Token; faellt er aus, greift fuer das Cover der iTunes-Fallback.
inline Info tidalLookup(const std::string& title, const std::string& artist) {
    Info info;
    std::string q = urlEncode(title + " " + artist);
    std::wstring path = L"/v1/search/tracks?query=" + utf8ToWide(q)
        + L"&limit=3&countryCode=US&token=CzET4vdadNUFQ5JU";

    std::string body = http::get(L"api.tidal.com", path);

    std::string cover = json::findString(body, "cover");
    if (!cover.empty()) {
        // Tidal-Cover-UUID -> Pfad: Bindestriche werden zu Slashes.
        std::string p;
        for (char c : cover) p += (c == '-') ? '/' : c;
        info.cover = "https://resources.tidal.com/images/" + p + "/640x640.jpg";
    }

    std::string url = json::findString(body, "url"); // erstes "url" = Track-Link
    if (!url.empty()) {
        if (url.rfind("http://", 0) == 0) url = "https://" + url.substr(7);
        info.tidalUrl = url;

        // Letztes Pfadsegment = numerische Track-ID (.../track/12345).
        size_t s = url.find_last_of('/');
        if (s != std::string::npos) {
            std::string id = url.substr(s + 1);
            bool digits = !id.empty();
            for (char c : id)
                if (!std::isdigit((unsigned char)c)) { digits = false; break; }
            if (digits) info.trackId = id;
        }
    }
    return info;
}

// iTunes Search API: oeffentlich, ohne Auth.
inline std::string itunesCover(const std::string& title, const std::string& artist) {
    std::string q = urlEncode(artist + " " + title);
    std::wstring path = L"/search?term=" + utf8ToWide(q) + L"&entity=song&limit=1";

    std::string body = http::get(L"itunes.apple.com", path);
    std::string art = json::findString(body, "artworkUrl100");
    if (art.empty()) return "";

    // 100x100 -> 600x600 hochskalieren.
    size_t pos = art.find("100x100");
    if (pos != std::string::npos) art.replace(pos, 7, "600x600");
    return art;
}

// Cover + Track-URL ermitteln. Cover faellt notfalls auf iTunes zurueck;
// die Track-URL gibt es nur ueber Tidal.
inline Info lookup(const std::string& title, const std::string& artist) {
    Info info = tidalLookup(title, artist);
    if (info.cover.empty())
        info.cover = itunesCover(title, artist);
    return info;
}

} // namespace tidal
