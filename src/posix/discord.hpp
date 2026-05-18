// Discord Rich Presence über lokalen Unix-Domain-Socket (Linux/macOS).
// Frame-Format identisch zu Windows: int32 opcode | int32 length | UTF-8 JSON.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

namespace discord {

enum Opcode : int32_t { OP_HANDSHAKE = 0, OP_FRAME = 1, OP_CLOSE = 2 };

enum class Result { Ok, NoPipe, Rejected };

class IPC {
    int fd = -1;

    // Versucht, einen Unix-Socket zu öffnen. Gibt fd zurück oder -1.
    static int tryOpen(const std::string& path) {
        if (path.size() >= sizeof(sockaddr_un::sun_path)) return -1;
        int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (s < 0) return -1;
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
            return s;
        ::close(s);
        return -1;
    }

public:
    std::string lastError;

    ~IPC() { close(); }

    bool connected() const { return fd >= 0; }

    void close() {
        if (fd >= 0) { ::close(fd); fd = -1; }
    }

    Result connect(const std::string& clientId) {
        // Discord legt den Socket je nach Umgebung in unterschiedliche Ordner.
        std::vector<std::string> bases;
        if (const char* x = getenv("XDG_RUNTIME_DIR")) bases.push_back(x);
        if (const char* t = getenv("TMPDIR"))          bases.push_back(t);
        bases.push_back("/tmp");

        const char* subs[] = {
            "", "app/com.discordapp.Discord/",
            "app/com.discordapp.DiscordCanary/", "snap.discord/"
        };

        for (const auto& base : bases) {
            for (const char* sub : subs) {
                for (int i = 0; i < 10; ++i) {
                    int s = tryOpen(base + "/" + sub
                                    + "discord-ipc-" + std::to_string(i));
                    if (s >= 0) { fd = s; break; }
                }
                if (connected()) break;
            }
            if (connected()) break;
        }
        if (!connected()) {
            lastError = "Discord-Socket nicht gefunden (läuft die Desktop-App?).";
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
        if (resp.find("READY") == std::string::npos
            && resp.find("DISPATCH") == std::string::npos) {
            lastError = "Discord lehnt die App-ID ab.";
            close();
            return Result::Rejected;
        }
        lastError.clear();
        return Result::Ok;
    }

    bool send(int32_t opcode, const std::string& payload) {
        if (!connected()) return false;
        int32_t len = static_cast<int32_t>(payload.size());
        std::string frame;
        frame.append(reinterpret_cast<const char*>(&opcode), 4);
        frame.append(reinterpret_cast<const char*>(&len), 4);
        frame += payload;
        return writeAll(frame.data(), frame.size());
    }

    bool readFrame(std::string& out) {
        char hdr[8];
        if (!readAll(hdr, 8)) return false;
        int32_t len = 0;
        std::memcpy(&len, hdr + 4, 4);
        if (len < 0 || len > (1 << 20)) return false;
        out.assign(static_cast<size_t>(len), '\0');
        return len == 0 || readAll(&out[0], static_cast<size_t>(len));
    }

private:
    bool writeAll(const char* p, size_t n) {
        size_t total = 0;
        while (total < n) {
            ssize_t w = ::write(fd, p + total, n - total);
            if (w <= 0) return false;
            total += static_cast<size_t>(w);
        }
        return true;
    }
    bool readAll(char* p, size_t n) {
        size_t total = 0;
        while (total < n) {
            ssize_t r = ::read(fd, p + total, n - total);
            if (r <= 0) return false;
            total += static_cast<size_t>(r);
        }
        return true;
    }
};

} // namespace discord
