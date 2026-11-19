#include "format.hpp"
#include <algorithm>
#include <cstdio>

std::string formatBytes(std::uint64_t bytes) {
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 5) { v /= 1024.0; ++u; }
    char buf[32];
    if (u == 0) std::snprintf(buf, sizeof buf, "%llu B", static_cast<unsigned long long>(bytes));
    else        std::snprintf(buf, sizeof buf, "%.1f %s", v, units[u]);
    return buf;
}

std::string progressBar(double percent, int width, bool color) {
    percent = std::clamp(percent, 0.0, 100.0);
    int filled = static_cast<int>(percent / 100.0 * width + 0.5);
    std::string bar = "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
    if (!color) return bar;
    const char* c = percent < 60 ? "\033[32m" : percent < 85 ? "\033[33m" : "\033[31m";
    return std::string(c) + bar + "\033[0m";
}
