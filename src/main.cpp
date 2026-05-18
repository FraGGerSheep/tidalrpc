// tidalrpc - Discord Rich Presence fuer Tidal.
// GUI-App ohne Konsole: laeuft als Icon im System-Tray.
#include <windows.h>
#include <shellapi.h>
#include "resource.h"
#include "media.hpp"
#include "discord.hpp"
#include "tidal.hpp"

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <cwchar>

// Discord Application ID.
static const std::string APP_ID = "1506031376997027940";

// Optional: URL der gehosteten redirect.html. Ist sie gesetzt, oeffnet der
// "Play on Tidal"-Button per https-Redirect die Tidal-Desktop-App.
// Leer lassen -> Button oeffnet stattdessen den Tidal-Webplayer im Browser.
// Beispiel: "https://DEINNAME.github.io/tidalrpc/redirect.html"
static const std::string REDIRECT_BASE = "";

#define WM_TRAYICON  (WM_APP + 1)
#define WM_STATUS    (WM_APP + 2)
#define ID_TRAY_TRACK 40001
#define ID_TRAY_OPEN  40002
#define ID_TRAY_QUIT  40003

static HWND               g_hwnd = nullptr;
static NOTIFYICONDATAW    g_nid  = {};
static std::atomic<bool>  g_running{ true };
static std::thread        g_worker;
static std::mutex         g_mtx;
static std::wstring       g_status  = L"Starte ...";
static std::wstring       g_trackId;             // numerische Track-ID oder leer
static unsigned long long g_nonce   = 0;

// --- Hilfsfunktionen ---------------------------------------------------------

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                        &s[0], n, nullptr, nullptr);
    return s;
}

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
                    sprintf_s(buf, sizeof(buf), "\\u%04x", c);
                    o += buf;
                } else {
                    o += (char)c;
                }
        }
    }
    return o;
}

// Auf Discords Feldgrenzen kappen (2..128 Zeichen).
static std::wstring clip(std::wstring s) {
    if (s.size() > 120) s.resize(120);
    while (s.size() < 2) s += L' ';
    return s;
}

// --- Presence ----------------------------------------------------------------

static void setActivity(discord::IPC& ipc, const media::NowPlaying& np,
                        const tidal::Info& ti) {
    std::wstring artistW = np.artist.empty() ? L"Unbekannter Interpret" : np.artist;
    if (!np.playing) artistW += L"  -  pausiert";

    std::string title  = jsonEscape(wideToUtf8(clip(np.title)));
    std::string artist = jsonEscape(wideToUtf8(clip(artistW)));
    std::string album  = jsonEscape(wideToUtf8(clip(np.album)));

    // name -> Anzeige "Listening to <name>" (neuere Discord-Clients).
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

    if (np.playing && np.endMs > np.startMs)
        a += ",\"timestamps\":{\"start\":" + std::to_string(np.startMs)
           + ",\"end\":" + std::to_string(np.endMs) + "}";

    // Button-Ziel: Redirect-Seite (oeffnet Desktop-App) bevorzugen,
    // sonst direkter Tidal-Webplayer-Link.
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
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"
        + std::to_string(GetCurrentProcessId())
        + ",\"activity\":" + a + "},\"nonce\":\""
        + std::to_string(++g_nonce) + "\"}";

    ipc.drain();
    if (!ipc.send(discord::OP_FRAME, payload))
        ipc.close();
}

static void clearActivity(discord::IPC& ipc) {
    if (!ipc.connected()) return;
    std::string payload =
        "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":"
        + std::to_string(GetCurrentProcessId())
        + "},\"nonce\":\"" + std::to_string(++g_nonce) + "\"}";
    ipc.drain();
    if (!ipc.send(discord::OP_FRAME, payload))
        ipc.close();
}

// --- Worker-Thread -----------------------------------------------------------

// Setzt den Tray-Tooltip.
static void publish(const std::wstring& status) {
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        g_status = status;
    }
    if (g_hwnd) PostMessageW(g_hwnd, WM_STATUS, 0, 0);
}

static bool interruptibleSleep(int ms) {
    for (int i = 0; i < ms && g_running; i += 100)
        Sleep(100);
    return g_running;
}

static void workerLoop() {
    winrt::init_apartment(); // SMTC/WinRT in diesem Thread initialisieren
    discord::IPC ipc;
    std::wstring lastKey;
    bool wasConnected = false;

    while (g_running) {
        if (!ipc.connected()) {
            discord::Result r = ipc.connect(APP_ID);
            if (r == discord::Result::Ok) {
                lastKey.clear();
                if (!wasConnected)
                    publish(L"Mit Discord verbunden");
                wasConnected = true;
            } else {
                publish(tidal::utf8ToWide(ipc.lastError));
                wasConnected = false;
                if (!interruptibleSleep(5000)) break;
                continue;
            }
        }

        media::NowPlaying np = media::poll();
        std::wstring key = np.valid
            ? np.title + L'\x1f' + np.artist + (np.playing ? L"|P" : L"|p")
            : L"";

        if (key != lastKey) {
            if (!np.valid) {
                clearActivity(ipc);
                { std::lock_guard<std::mutex> lk(g_mtx); g_trackId.clear(); }
                publish(L"Tidal: nichts laeuft");
            } else if (!np.playing) {
                // Pausiert -> Presence ausblenden (wie Spotify-RPC).
                clearActivity(ipc);
                publish(np.artist + L" - " + np.title + L"  (pausiert)");
            } else {
                tidal::Info ti = tidal::lookup(
                    wideToUtf8(np.title), wideToUtf8(np.artist));
                setActivity(ipc, np, ti);
                {
                    std::lock_guard<std::mutex> lk(g_mtx);
                    g_trackId = tidal::utf8ToWide(ti.trackId);
                }
                publish(np.artist + L" - " + np.title);
            }
            lastKey = key;
        }

        if (!interruptibleSleep(3000)) break;
    }

    clearActivity(ipc);
    ipc.close();
}

// --- Tray / Fenster ----------------------------------------------------------

// Prueft, ob die Tidal-Desktop-App das tidal://-Protokoll registriert hat.
static bool tidalInstalled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"tidal\\shell\\open\\command",
                      0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

// Aktuellen Track oeffnen: Desktop-App wenn installiert, sonst Webplayer.
static void openCurrentTrack() {
    std::wstring id;
    {
        std::lock_guard<std::mutex> lk(g_mtx);
        id = g_trackId;
    }
    if (id.empty()) return;
    std::wstring target = tidalInstalled()
        ? L"tidal://track/" + id
        : L"https://tidal.com/browse/track/" + id;
    ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_STATUS: {
        std::wstring status;
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            status = g_status;
        }
        std::wstring tip = L"tidalrpc - " + status;
        if (tip.size() > 127) tip.resize(127);
        wcscpy_s(g_nid.szTip, tip.c_str());
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        return 0;
    }
    case WM_TRAYICON: {
        WORD ev = LOWORD(l);
        if (ev == WM_LBUTTONDBLCLK) {
            // Doppelklick -> aktuellen Track oeffnen.
            openCurrentTrack();
        } else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
            std::wstring status;
            bool hasTrack;
            {
                std::lock_guard<std::mutex> lk(g_mtx);
                status = g_status;
                hasTrack = !g_trackId.empty();
            }
            if (status.size() > 100) status.resize(100);
            // Label spiegelt, ob die Desktop-App installiert ist.
            const wchar_t* openLabel = tidalInstalled()
                ? L"In Tidal-App oeffnen"
                : L"Im Tidal-Webplayer oeffnen";
            POINT pt;
            GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING | MF_GRAYED, ID_TRAY_TRACK, status.c_str());
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | (hasTrack ? 0 : MF_GRAYED),
                        ID_TRAY_OPEN, openLabel);
            AppendMenuW(menu, MF_STRING, ID_TRAY_QUIT, L"Beenden");
            SetForegroundWindow(h); // sonst schliesst das Menue nicht
            TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                           pt.x, pt.y, 0, h, nullptr);
            DestroyMenu(menu);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == ID_TRAY_QUIT)
            DestroyWindow(h);
        else if (LOWORD(w) == ID_TRAY_OPEN)
            openCurrentTrack();
        return 0;
    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Nur eine Instanz zulassen.
    HANDLE single = CreateMutexW(nullptr, FALSE, L"tidalrpc_singleton");
    if (single && GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"tidalrpcWndClass";
    RegisterClassW(&wc);

    // Verstecktes Fenster (nie ShowWindow) - traegt das Tray-Icon.
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"tidalrpc",
        WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);

    HICON icon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY));
    if (!icon) icon = LoadIconW(nullptr, IDI_APPLICATION);

    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = icon;
    wcscpy_s(g_nid.szTip, L"tidalrpc - Starte ...");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    g_worker = std::thread(workerLoop);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_running = false;
    if (g_worker.joinable())
        g_worker.join();
    if (single) CloseHandle(single);
    return 0;
}
