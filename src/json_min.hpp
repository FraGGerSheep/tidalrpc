#pragma once
#include <string>

namespace json {

inline std::string findString(const std::string& json, const std::string& key) {
    const std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat);
    if (k == std::string::npos) return "";

    size_t i = k + pat.size();
    auto skipWs = [&] { while (i < json.size() &&
        (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r')) ++i; };

    skipWs();
    if (i >= json.size() || json[i] != ':') return "";
    ++i;
    skipWs();
    if (i >= json.size() || json[i] != '"') return "";
    ++i;

    std::string out;
    while (i < json.size() && json[i] != '"') {
        if (json[i] == '\\' && i + 1 < json.size()) {
            char n = json[i + 1];
            switch (n) {
                case 'n':  out += '\n'; break;
                case 't':  out += '\t'; break;
                case 'r':  out += '\r'; break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'u':  if (i + 5 < json.size()) i += 4; break;
                default:   out += n;    break;
            }
            i += 2;
        } else {
            out += json[i];
            ++i;
        }
    }
    return out;
}

}
