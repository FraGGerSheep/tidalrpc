// HTTPS-GET ueber WinHTTP (Windows-eigen, keine externe Abhaengigkeit).
#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>

namespace http {

// GET auf host+path. Gibt den Body zurueck, "" bei Fehler.
inline std::string get(const std::wstring& host, const std::wstring& path, bool https = true) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"tidalrpc/1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (hConnect) {
        DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (hRequest) {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                && WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD avail = 0;
                do {
                    avail = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
                    if (avail) {
                        std::string buf(avail, '\0');
                        DWORD read = 0;
                        if (WinHttpReadData(hRequest, &buf[0], avail, &read)) {
                            buf.resize(read);
                            result += buf;
                        }
                    }
                } while (avail > 0);
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return result;
}

} // namespace http
