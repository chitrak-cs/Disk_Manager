#include "utils.hpp"
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::optional<long long> readFirstLineAsInt(const std::string& path) {
    auto txt = readFile(path);
    if (!txt) return std::nullopt;
    std::string t = trim(*txt);
    long long v = 0;
    auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc{} || p == t.data()) return std::nullopt;
    return v;
}

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t i = s.find(delim, start);
        if (i == std::string_view::npos) { out.emplace_back(s.substr(start)); break; }
        out.emplace_back(s.substr(start, i - start));
        start = i + 1;
    }
    return out;
}

std::vector<std::string> splitWhitespace(std::string_view s) {
    std::vector<std::string> out;
    std::istringstream is{std::string(s)};
    std::string tok;
    while (is >> tok) out.push_back(tok);
    return out;
}
