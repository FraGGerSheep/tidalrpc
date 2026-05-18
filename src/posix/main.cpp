// tidalrpc - Discord Rich Presence für Tidal (Linux/macOS, Terminal).
#include "discord.hpp"
#include "http.hpp"
#include "tidal.hpp"
#include "media.hpp"

#include <string>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <unistd.h>

static const std::string APP_ID = "1506031376997027940";

// URL der gehosteten redirect.html für den "Play on Tidal"-Button.
static const std::string REDIRECT_BASE =
    "https://fraggersheep.github.io/tidalrpc/redirect.html";

static volatile sig_atomic_t g_run = 1;
static void onSignal(int) { g_run = 0; }

static unsigned long long g_nonce = 0;

// --- Hilfsfunktionen ---------------------------------------------------------

static std::string jsonEscape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\b': o += "\\b";  break;
            case '\f': o += "\\f";  break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

// Auf Discords Feldgrenzen kappen, ohne ein UTF-8-Zeichen zu zerschneiden.
static std::string clip(std::string s) {
    if (s.size() > 120) {
        s.resize(120);
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80)
            s.pop_back();
        if (!s.empty() && (static_cast<unsigned char>(s.back()) & 0x80))
            s.pop_back();
    }
    while (s.size() < 2) s += ' ';
    return s;
}

static long long nowMs() {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

// --- Presence ----------------------------------------------------------------

static void setActivity(discord::IPC& ipc, const media::NowPlaying& np,
                        const tidal::Info& ti) {
    std::string title  = jsonEscape(clip(np.title));
    std::string artist = jsonEscape(clip(np.artist.empty()
                                         ? "Unbekannter Interpret" : np.artist));
    std::string album  = jsonEscape(clip(np.album));

    std::string a = "{\"name\":\"Tidal\",\"type\":2,\"details\":\"" + title
                  + "\",\"state\":\"" + artist + "\"";

    std::string assets;
    if (!ti.cover.empty()) {
        assets = "\"large_image\":\"" + jsonEscape(ti.cover) + "\"";
        if (!np.album.empty())
            assets += ",\"large_text\":\"" + album + "\"";
    } else if (!np.album.empty()) {
        assets = "\"large_text\":\"" + album + "\"";
    }
    if (!assets.empty())
        a += ",\"assets\":{" + assets + "}";

    if (np.playing && np.lengthMs > 0) {
        long long start = nowMs() - np.positionMs;
        a += ",\"timestamps\":{\"start\":" + std::to_string(start)
           + ",\"end\":" + std::to_string(start + np.lengthMs) + "}";
    }

    std::string buttonUrl;
    if (!ti.trackId.empty()) {
        buttonUrl = REDIRECT_BASE.empty()
            ? "https://tidal.com/browse/track/" + ti.trackId
            : REDIRECT_BASE + "?id=" + ti.trackId;
    } else if (!ti.tidalUrl.empty()) {
        buttonUrl = ti.tidalUrl;
    }
    if (!buttonUrl.empty())
        a += ",\"buttons\":[{\"label\":\"Play on Tidal\",\"url\":\""
           + jsonEscape(buttonUrl) + "\"}]";
    a += "}";

    std::string payload =
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(getpid())
        + ",\"activity\":" + a + "},\"nonce\":\""
        + std::to_string(++g_nonce) + "\"}";

    if (!ipc.send(discord::OP_FRAME, payload))
        ipc.close();
}

static void clearActivity(discord::IPC& ipc) {
    if (!ipc.connected()) return;
    std::string payload =
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(getpid())
        + "},\"nonce\":\"" + std::to_string(++g_nonce) + "\"}";
    ipc.send(discord::OP_FRAME, payload);
}

// Schlaf in 100-ms-Schritten, abbrechbar über das Signal.
static bool nap(int ms) {
    for (int i = 0; i < ms && g_run; i += 100)
        usleep(100000);
    return g_run;
}

// --- main --------------------------------------------------------------------

int main() {
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("tidalrpc - Discord Rich Presence für Tidal\n"
           "Beenden mit Strg+C.\n\n");

    discord::IPC ipc;
    std::string lastKey;

    while (g_run) {
        if (!ipc.connected()) {
            if (ipc.connect(APP_ID) == discord::Result::Ok) {
                lastKey.clear();
                printf("[+] Mit Discord verbunden.\n");
            } else {
                printf("[!] %s\n", ipc.lastError.c_str());
                fflush(stdout);
                if (!nap(5000)) break;
                continue;
            }
            fflush(stdout);
        }

        media::NowPlaying np = media::poll();
        std::string key = np.valid
            ? np.title + "\x1f" + np.artist + (np.playing ? "|P" : "|p")
            : "";

        if (key != lastKey) {
            if (!np.valid) {
                clearActivity(ipc);
                printf("[ ] Tidal: nichts läuft\n");
            } else if (!np.playing) {
                clearActivity(ipc);
                printf("[=] %s - %s  (pausiert)\n",
                       np.artist.c_str(), np.title.c_str());
            } else {
                tidal::Info ti = tidal::lookup(np.title, np.artist);
                setActivity(ipc, np, ti);
                printf("[>] %s - %s\n", np.artist.c_str(), np.title.c_str());
            }
            fflush(stdout);
            lastKey = key;
        }

        if (!nap(3000)) break;
    }

    clearActivity(ipc);
    ipc.close();
    curl_global_cleanup();
    printf("\nbeendet.\n");
    return 0;
}
