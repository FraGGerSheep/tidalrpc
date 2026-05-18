// Aktuell laufender Track — Schnittstelle. Implementierung je Plattform:
//   Linux  -> media_linux.cpp (MPRIS via playerctl)
//   macOS  -> media_mac.mm    (MediaRemote-Framework)
#pragma once
#include <string>

namespace media {

struct NowPlaying {
    bool valid = false;
    bool playing = false;
    std::string title, artist, album;
    long long lengthMs = 0;   // Track-Länge
    long long positionMs = 0; // aktuelle Position
};

NowPlaying poll();

} // namespace media
