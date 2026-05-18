// Aktueller Tidal-Track ueber System Media Transport Controls (C++/WinRT).
#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <string>
#include <chrono>
#include <cwctype>

namespace media {

using namespace winrt::Windows::Media::Control;

struct NowPlaying {
    bool valid   = false;
    bool playing = false;
    std::wstring title, artist, album;
    long long startMs = 0; // Unix-ms: Track-Anfang
    long long endMs   = 0; // Unix-ms: Track-Ende
};

// Liest die SMTC-Sitzung, deren App-ID "tidal" enthaelt.
inline NowPlaying poll() {
    NowPlaying np;
    try {
        auto mgr = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

        GlobalSystemMediaTransportControlsSession session{ nullptr };
        for (auto&& s : mgr.GetSessions()) {
            winrt::hstring id = s.SourceAppUserModelId();
            std::wstring lower(id.c_str(), id.size());
            for (auto& c : lower) c = (wchar_t)std::towlower(c);
            if (lower.find(L"tidal") != std::wstring::npos) {
                session = s;
                break;
            }
        }
        if (!session) return np; // Tidal laeuft nicht / spielt nichts

        auto props = session.TryGetMediaPropertiesAsync().get();
        np.title  = std::wstring(props.Title().c_str(),      props.Title().size());
        np.artist = std::wstring(props.Artist().c_str(),     props.Artist().size());
        np.album  = std::wstring(props.AlbumTitle().c_str(), props.AlbumTitle().size());

        auto info = session.GetPlaybackInfo();
        np.playing = info.PlaybackStatus()
            == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

        auto tl = session.GetTimelineProperties();
        auto posMs = std::chrono::duration_cast<std::chrono::milliseconds>(tl.Position()).count();
        auto endMs = std::chrono::duration_cast<std::chrono::milliseconds>(tl.EndTime()).count();
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        np.startMs = nowMs - posMs;
        if (endMs > posMs) np.endMs = nowMs + (endMs - posMs);

        np.valid = !np.title.empty();
    } catch (...) {
        np.valid = false;
    }
    return np;
}

} // namespace media
