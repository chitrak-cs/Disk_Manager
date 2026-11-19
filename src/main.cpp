#include "disk.hpp"
#include "format.hpp"
#include "gpu.hpp"
#include "memory.hpp"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace {
struct Options { bool disk = true, ram = true, gpu = true, json = false; int watch = 0; };

void usage() {
    std::cout << "sysmon - system snapshot\n"
                 "  --disk | --ram | --gpu   show only the chosen section(s)\n"
                 "  --watch N                refresh every N seconds\n"
                 "  --json                   machine-readable output\n"
                 "  --color | --no-color     force or disable ANSI colours (default: auto)\n"
                 "  --help\n";
}

double pct(std::uint64_t part, std::uint64_t whole) { return whole ? 100.0 * part / whole : 0.0; }

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) { if (c == '"' || c == '\\') o += '\\'; if (c >= 0x20) o += c; }
    return o;
}
template <class T> std::string jopt(const std::optional<T>& v) { return v ? std::to_string(*v) : "null"; }

void printText(const Options& o, bool color) {
    char host[256] = "?";
    gethostname(host, sizeof host);
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "System Monitor v0.1   " << host << "   " << std::put_time(std::localtime(&now), "%F %T") << "\n";

    if (o.disk) {
        std::cout << "\n== STORAGE ==\n";
        std::cout << std::left << std::setw(18) << "Device" << std::setw(22) << "Mount" << std::setw(8) << "Type"
                  << std::right << std::setw(11) << "Size" << std::setw(11) << "Used" << std::setw(11) << "Free"
                  << "  " << "Use%\n";
        for (const auto& d : collectDisks()) {
            double p = pct(d.used, d.used + d.available);   // df-style Use%
            std::cout << std::left << std::setw(18) << d.device.substr(0, 17) << std::setw(22) << d.mountPoint.substr(0, 21)
                      << std::setw(8) << d.fsType << std::right << std::setw(11) << formatBytes(d.total)
                      << std::setw(11) << formatBytes(d.used) << std::setw(11) << formatBytes(d.available)
                      << "  " << progressBar(p, 10, color) << " " << std::fixed << std::setprecision(0) << p << "%"
                      << (d.isSsd ? (*d.isSsd ? "  SSD" : "  HDD") : "") << "\n";
        }
    }
    if (o.ram) {
        auto m = collectMemory();
        std::cout << "\n== MEMORY ==\n"
                  << "RAM   " << progressBar(pct(m.used, m.total), 20, color) << " " << formatBytes(m.used) << " / "
                  << formatBytes(m.total) << "  (available " << formatBytes(m.available)
                  << ", buffers " << formatBytes(m.buffers) << ", cache " << formatBytes(m.cached) << ")\n";
        if (m.swapTotal)
            std::cout << "Swap  " << progressBar(pct(m.swapUsed, m.swapTotal), 20, color) << " "
                      << formatBytes(m.swapUsed) << " / " << formatBytes(m.swapTotal) << "\n";
        else std::cout << "Swap  none\n";
    }
    if (o.gpu) {
        std::cout << "\n== GPU ==\n";
        for (const auto& g : collectGpus()) {
            std::cout << g.id << ": " << g.vendor << " " << g.name;
            if (!g.note.empty()) std::cout << "  (" << g.note << ")";
            std::cout << "\n    util: ";
            if (g.utilPercent) std::cout << progressBar(*g.utilPercent, 20, color) << " " << *g.utilPercent << "%"; else std::cout << "N/A";
            std::cout << "   VRAM: ";
            if (g.vramUsed && g.vramTotal) std::cout << formatBytes(*g.vramUsed) << " / " << formatBytes(*g.vramTotal); else std::cout << "N/A";
            std::cout << "   temp: "; if (g.tempC) std::cout << *g.tempC << " C"; else std::cout << "N/A";
            std::cout << "   power: "; if (g.powerW) std::cout << std::fixed << std::setprecision(1) << *g.powerW << " W"; else std::cout << "N/A";
            std::cout << "\n";
        }
    }
}

void printJson(const Options& o) {
    std::cout << "{";
    bool first = true;
    auto sep = [&] { if (!first) std::cout << ","; first = false; };
    if (o.disk) {
        sep(); std::cout << "\"disks\":[";
        bool f = true;
        for (const auto& d : collectDisks()) {
            if (!f) { std::cout << ","; }
            f = false;
            std::cout << "{\"device\":\"" << jsonEscape(d.device) << "\",\"mount\":\"" << jsonEscape(d.mountPoint)
                      << "\",\"fs\":\"" << d.fsType << "\",\"total\":" << d.total << ",\"used\":" << d.used
                      << ",\"available\":" << d.available << ",\"ssd\":" << (d.isSsd ? (*d.isSsd ? "true" : "false") : "null") << "}";
        }
        std::cout << "]";
    }
    if (o.ram) {
        auto m = collectMemory();
        sep(); std::cout << "\"memory\":{\"total\":" << m.total << ",\"used\":" << m.used << ",\"available\":" << m.available
                         << ",\"buffers\":" << m.buffers << ",\"cached\":" << m.cached << ",\"swapTotal\":" << m.swapTotal
                         << ",\"swapUsed\":" << m.swapUsed << "}";
    }
    if (o.gpu) {
        sep(); std::cout << "\"gpus\":[";
        bool f = true;
        for (const auto& g : collectGpus()) {
            if (!f) { std::cout << ","; }
            f = false;
            std::cout << "{\"id\":\"" << g.id << "\",\"vendor\":\"" << g.vendor << "\",\"name\":\"" << jsonEscape(g.name)
                      << "\",\"util\":" << jopt(g.utilPercent) << ",\"vramUsed\":" << jopt(g.vramUsed)
                      << ",\"vramTotal\":" << jopt(g.vramTotal) << ",\"tempC\":" << jopt(g.tempC) << "}";
        }
        std::cout << "]";
    }
    std::cout << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
    Options o;
    bool picked = false;
    int colorMode = -1;   // -1 auto, 0 off, 1 on
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto pick = [&](bool& flag) { if (!picked) { o.disk = o.ram = o.gpu = false; picked = true; } flag = true; };
        if (a == "--disk") pick(o.disk);
        else if (a == "--ram") pick(o.ram);
        else if (a == "--gpu") pick(o.gpu);
        else if (a == "--json") o.json = true;
        else if (a == "--color") colorMode = 1;
        else if (a == "--no-color") colorMode = 0;
        else if (a == "--watch" && i + 1 < argc) o.watch = std::max(1, std::atoi(argv[++i]));
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { std::cerr << "unknown option: " << a << "\n"; usage(); return 2; }
    }
    bool tty = isatty(STDOUT_FILENO);
    bool color = colorMode < 0 ? tty : colorMode == 1;
    do {
        if (o.watch && tty) std::cout << "\033[2J\033[H";
        o.json ? printJson(o) : printText(o, color);
        std::cout.flush();
        if (o.watch) std::this_thread::sleep_for(std::chrono::seconds(o.watch));
    } while (o.watch);
}
