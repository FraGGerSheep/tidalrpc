#include "media.hpp"
#include <cstdio>
#include <cstdlib>
#include <cctype>

namespace {

std::string run(const std::string& cmd) {
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

}

namespace media {

NowPlaying poll() {
    NowPlaying np;

    std::string list = run("playerctl -l");
    if (list.empty()) return np;

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

    std::string len = run(q + "metadata mpris:length");
    if (!len.empty()) np.lengthMs = atoll(len.c_str()) / 1000;

    std::string pos = run(q + "position");
    if (!pos.empty())
        np.positionMs = static_cast<long long>(atof(pos.c_str()) * 1000);

    np.valid = !np.title.empty();
    return np;
}

}
