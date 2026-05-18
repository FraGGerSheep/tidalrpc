// Discord Rich Presence ueber lokale IPC-Named-Pipe.
// Frame-Format: int32 opcode (LE) | int32 length (LE) | UTF-8 JSON.
#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <cstring>

namespace discord {

enum Opcode : int32_t { OP_HANDSHAKE = 0, OP_FRAME = 1, OP_CLOSE = 2 };

enum class Result { Ok, NoPipe, Rejected };

class IPC {
    HANDLE pipe = INVALID_HANDLE_VALUE;

public:
    std::string lastError; // gefuellt bei fehlgeschlagenem connect()

    ~IPC() { close(); }

    bool connected() const { return pipe != INVALID_HANDLE_VALUE; }

    void close() {
        if (connected()) {
            CloseHandle(pipe);
            pipe = INVALID_HANDLE_VALUE;
        }
    }

    // Verbindet mit dem laufenden Discord-Desktop-Client und macht den Handshake.
    Result connect(const std::string& clientId) {
        bool pipeBusy = false;
        for (int i = 0; i < 10; ++i) {
            std::wstring name = L"\\\\?\\pipe\\discord-ipc-" + std::to_wstring(i);
            HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h == INVALID_HANDLE_VALUE) {
                if (GetLastError() == ERROR_PIPE_BUSY) {
                    pipeBusy = true;
                    if (WaitNamedPipeW(name.c_str(), 2000)) { --i; continue; }
                }
                continue;
            }
            pipe = h;
            break;
        }
        if (!connected()) {
            lastError = pipeBusy
                ? "Discord-Pipe belegt - kurz warten."
                : "Discord-Desktop-App nicht gefunden (Browser-Discord hat keine Pipe).";
            return Result::NoPipe;
        }

        std::string handshake = "{\"v\":1,\"client_id\":\"" + clientId + "\"}";
        if (!send(OP_HANDSHAKE, handshake)) {
            lastError = "Handshake-Senden fehlgeschlagen.";
            close();
            return Result::Rejected;
        }

        std::string resp;
        if (!readFrame(resp)) {
            lastError = "Keine Antwort von Discord auf den Handshake.";
            close();
            return Result::Rejected;
        }
        // Erfolg = READY-Dispatch. Sonst meist falsche/unbekannte Client-ID.
        if (resp.find("READY") == std::string::npos
            && resp.find("DISPATCH") == std::string::npos) {
            lastError = "Discord lehnt die App-ID ab: " + resp.substr(0, 180);
            close();
            return Result::Rejected;
        }

        lastError.clear();
        return Result::Ok;
    }

    bool send(int32_t opcode, const std::string& payload) {
        if (!connected()) return false;
        int32_t len = (int32_t)payload.size();
        std::string frame;
        frame.append(reinterpret_cast<char*>(&opcode), 4);
        frame.append(reinterpret_cast<char*>(&len), 4);
        frame.append(payload);

        DWORD written = 0;
        return WriteFile(pipe, frame.data(), (DWORD)frame.size(), &written, nullptr)
            && written == frame.size();
    }

    // Liest einen kompletten Frame; gibt den JSON-Body zurueck.
    bool readFrame(std::string& out) {
        if (!connected()) return false;
        char hdr[8];
        if (!readExact(hdr, 8)) return false;

        int32_t len = 0;
        std::memcpy(&len, hdr + 4, 4);
        if (len < 0 || len > (1 << 20)) return false;
        out.assign((size_t)len, '\0');
        return len == 0 || readExact(&out[0], len);
    }

    // Wartende Antwort-Frames leeren, damit der Pipe-Puffer nicht volllaeuft.
    void drain() {
        if (!connected()) return;
        DWORD avail = 0;
        while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr) && avail >= 8) {
            std::string tmp;
            if (!readFrame(tmp)) break;
        }
    }

private:
    // ReadFile auf einer Byte-Pipe kann teilweise lesen -> bis voll lesen.
    bool readExact(void* buf, int n) {
        char* p = static_cast<char*>(buf);
        int total = 0;
        while (total < n) {
            DWORD got = 0;
            if (!ReadFile(pipe, p + total, n - total, &got, nullptr) || got == 0)
                return false;
            total += (int)got;
        }
        return true;
    }
};

} // namespace discord
