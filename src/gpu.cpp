#include "gpu.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<GpuInfo> collectGpus() {
    std::vector<GpuInfo> out;
    std::vector<std::string> cards;
    std::error_code ec;
    for (auto& e : fs::directory_iterator("/sys/class/drm", ec)) {
        std::string n = e.path().filename().string();
        // keep "card<digits>" only; skip "card1-DP-1" connectors and renderD*
        if (n.size() > 4 && n.rfind("card", 0) == 0 &&
            std::all_of(n.begin() + 4, n.end(), [](unsigned char c) { return std::isdigit(c); }))
            cards.push_back(n);
    }
    std::sort(cards.begin(), cards.end());

    bool nvidiaDone = false;
    for (const auto& c : cards) {
        std::string dir = "/sys/class/drm/" + c + "/device";
        std::string vendor = trim(readFile(dir + "/vendor").value_or(""));
        if (vendor == "0x1002") {
            out.push_back(readAmdGpu(dir, c));
        } else if (vendor == "0x10de") {
            if (nvidiaDone) continue;           // NVML enumerates all NVIDIA GPUs at once
            nvidiaDone = true;
            auto g = readNvidiaGpus();
            out.insert(out.end(), g.begin(), g.end());
        } else {
            GpuInfo g; g.id = c;
            g.vendor = vendor == "0x8086" ? "Intel" : "Unknown (" + vendor + ")";
            g.name = c; g.note = "no reader for this vendor";
            out.push_back(g);
        }
    }
    return out;
}
