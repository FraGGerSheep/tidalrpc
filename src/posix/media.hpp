#pragma once
#include <string>

namespace media {

struct NowPlaying {
    bool valid = false;
    bool playing = false;
    std::string title, artist, album;
    long long lengthMs = 0;
    long long positionMs = 0;
};

NowPlaying poll();

}
