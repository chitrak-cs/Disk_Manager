#include "gpu.hpp"
#include "utils.hpp"
#include <filesystem>

namespace fs = std::filesystem;

GpuInfo readAmdGpu(const std::string& dir, const std::string& card) {
    GpuInfo g;
    g.id = card; g.vendor = "AMD";
    auto name = readFile(dir + "/product_name");
    g.name = name ? trim(*name) : "AMD GPU (" + trim(readFile(dir + "/device").value_or("?")) + ")";
    if (auto v = readFirstLineAsInt(dir + "/gpu_busy_percent")) g.utilPercent = static_cast<int>(*v);
    if (auto v = readFirstLineAsInt(dir + "/mem_info_vram_used"))  g.vramUsed = static_cast<std::uint64_t>(*v);
    if (auto v = readFirstLineAsInt(dir + "/mem_info_vram_total")) g.vramTotal = static_cast<std::uint64_t>(*v);
    std::error_code ec;
    for (auto& h : fs::directory_iterator(dir + "/hwmon", ec)) {
        if (auto t = readFirstLineAsInt((h.path() / "temp1_input").string())) { g.tempC = static_cast<int>(*t / 1000); }
        if (auto p = readFirstLineAsInt((h.path() / "power1_average").string())) { g.powerW = *p / 1e6; }
        else if (auto p2 = readFirstLineAsInt((h.path() / "power1_input").string())) { g.powerW = *p2 / 1e6; }
        if (g.tempC) break;
    }
    return g;
}
