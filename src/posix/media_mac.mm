#include "media.hpp"
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <dispatch/dispatch.h>
#include <cstring>

namespace {

typedef void (*MRGetNowPlayingInfoFn)(dispatch_queue_t, void (^)(CFDictionaryRef));
typedef void (*MRGetIsPlayingFn)(dispatch_queue_t, void (^)(Boolean));

std::string cfToStd(CFStringRef s) {
    if (!s) return "";
    CFIndex len = CFStringGetLength(s);
    CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::string out(static_cast<size_t>(max), '\0');
    if (CFStringGetCString(s, &out[0], max, kCFStringEncodingUTF8)) {
        out.resize(std::strlen(out.c_str()));
        return out;
    }
    return "";
}

double cfNum(CFNumberRef n) {
    double d = 0;
    if (n) CFNumberGetValue(n, kCFNumberDoubleType, &d);
    return d;
}

}

namespace media {

NowPlaying poll() {
    NowPlaying np;

    static void* handle = dlopen(
        "/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote",
        RTLD_LAZY);
    if (!handle) return np;

    auto getInfo = reinterpret_cast<MRGetNowPlayingInfoFn>(
        dlsym(handle, "MRMediaRemoteGetNowPlayingInfo"));
    auto getPlaying = reinterpret_cast<MRGetIsPlayingFn>(
        dlsym(handle, "MRMediaRemoteGetNowPlayingApplicationIsPlaying"));
    if (!getInfo) return np;

    dispatch_queue_t q =
        dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0);

    __block NowPlaying result;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    getInfo(q, ^(CFDictionaryRef info) {
        if (info) {
            auto* t  = (CFStringRef)CFDictionaryGetValue(
                info, CFSTR("kMRMediaRemoteNowPlayingInfoTitle"));
            auto* a  = (CFStringRef)CFDictionaryGetValue(
                info, CFSTR("kMRMediaRemoteNowPlayingInfoArtist"));
            auto* al = (CFStringRef)CFDictionaryGetValue(
                info, CFSTR("kMRMediaRemoteNowPlayingInfoAlbum"));
            auto* dur = (CFNumberRef)CFDictionaryGetValue(
                info, CFSTR("kMRMediaRemoteNowPlayingInfoDuration"));
            auto* ela = (CFNumberRef)CFDictionaryGetValue(
                info, CFSTR("kMRMediaRemoteNowPlayingInfoElapsedTime"));
            result.title  = cfToStd(t);
            result.artist = cfToStd(a);
            result.album  = cfToStd(al);
            result.lengthMs   = static_cast<long long>(cfNum(dur) * 1000);
            result.positionMs = static_cast<long long>(cfNum(ela) * 1000);
            result.valid = !result.title.empty();
        }
        dispatch_semaphore_signal(sem);
    });
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

    if (result.valid && getPlaying) {
        __block bool playing = false;
        dispatch_semaphore_t sem2 = dispatch_semaphore_create(0);
        getPlaying(q, ^(Boolean p) {
            playing = p;
            dispatch_semaphore_signal(sem2);
        });
        dispatch_semaphore_wait(sem2, DISPATCH_TIME_FOREVER);
        result.playing = playing;
    }
    return result;
}

}
