#pragma once
#include <string>
#include <curl/curl.h>

namespace http {

inline size_t writeCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    static_cast<std::string*>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

inline std::string get(const std::string& url) {
    std::string body;
    CURL* c = curl_easy_init();
    if (!c) return body;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "tidalrpc/1.0");
    if (curl_easy_perform(c) != CURLE_OK)
        body.clear();
    curl_easy_cleanup(c);
    return body;
}

}
