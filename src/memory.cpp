#include "memory.hpp"
#include "utils.hpp"
#include <map>
#include <sstream>

MemoryInfo parseMeminfo(const std::string& text) {
    std::map<std::string, std::uint64_t> kv;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto parts = splitWhitespace(line.substr(colon + 1));
        if (parts.empty()) continue;
        try { kv[line.substr(0, colon)] = std::stoull(parts[0]) * 1024; }  // kB -> bytes
        catch (...) { continue; }
    }
    MemoryInfo m;
    m.total = kv["MemTotal"];
    m.available = kv.count("MemAvailable") ? kv["MemAvailable"] : kv["MemFree"];
    m.used = m.total > m.available ? m.total - m.available : 0;
    m.buffers = kv["Buffers"];
    m.cached = kv["Cached"];
    m.swapTotal = kv["SwapTotal"];
    std::uint64_t swapFree = kv["SwapFree"];
    m.swapUsed = m.swapTotal > swapFree ? m.swapTotal - swapFree : 0;
    return m;
}

MemoryInfo collectMemory() {
    return parseMeminfo(readFile("/proc/meminfo").value_or(""));
}
