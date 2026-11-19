#pragma once
#include <cstdint>
#include <optional>
#include <string>

struct DiskInfo {
    std::string device, mountPoint, fsType;
    std::uint64_t total = 0, used = 0, available = 0;
    std::optional<bool> isSsd;   // unknown for e.g. network/virtual devices
};

struct MemoryInfo {
    std::uint64_t total = 0, used = 0, available = 0, buffers = 0, cached = 0;
    std::uint64_t swapTotal = 0, swapUsed = 0;
};

struct GpuInfo {
    std::string name, vendor, id;
    std::optional<int> utilPercent, tempC;
    std::optional<std::uint64_t> vramUsed, vramTotal;
    std::optional<double> powerW;
    std::string note;            // e.g. "driver not available"
};
