// Aktuell laufender Tidal-Track unter Linux — gelesen über MPRIS.
// Nutzt das Standard-CLI `playerctl` (Paket: playerctl).
#pragma once
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace media {

struct NowPlaying {
    bool valid = false;
    bool playing = false;
    std::string title, artist, album;
    long long lengthMs = 0;   // Track-Länge
    long long positionMs = 0; // aktuelle Position
};

// Führt ein Kommando aus und gibt stdout zurück (ohne Zeilenende).
inline std::string run(const std::string& cmd) {
    std::string out;
    FILE* p = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!p) return out;
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0)
        out.append(buf, n);
    pclose(p);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

inline NowPlaying poll() {
    NowPlaying np;

    std::string list = run("playerctl -l");
    if (list.empty()) return np; // kein Player aktiv / playerctl fehlt

    // Player suchen, dessen Track-URL oder Name auf Tidal deutet.
    std::string chosen;
    size_t start = 0;
    while (start <= list.size()) {
        size_t nl = list.find('\n', start);
        std::string name = list.substr(
            start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!name.empty()) {
            std::string url = run("playerctl -p '" + name
                                  + "' metadata xesam:url");
            std::string lname = name;
            for (auto& c : lname) c = static_cast<char>(std::tolower(c));
            if (url.find("tidal") != std::string::npos
                || lname.find("tidal") != std::string::npos) {
                chosen = name;
                break;
            }
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    if (chosen.empty()) return np;

    std::string q = "playerctl -p '" + chosen + "' ";
    np.title  = run(q + "metadata xesam:title");
    np.artist = run(q + "metadata xesam:artist");
    np.album  = run(q + "metadata xesam:album");
    np.playing = (run(q + "status") == "Playing");

    std::string len = run(q + "metadata mpris:length"); // Mikrosekunden
    if (!len.empty()) np.lengthMs = atoll(len.c_str()) / 1000;

    std::string pos = run(q + "position"); // Sekunden (Fließkomma)
    if (!pos.empty()) np.positionMs = static_cast<long long>(atof(pos.c_str()) * 1000);

    np.valid = !np.title.empty();
    return np;
}

} // namespace media
