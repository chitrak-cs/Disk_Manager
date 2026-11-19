#pragma once
#include "types.hpp"
#include <vector>

std::vector<GpuInfo> collectGpus();
// vendor readers
GpuInfo readAmdGpu(const std::string& cardDir, const std::string& cardName);
std::vector<GpuInfo> readNvidiaGpus();   // via NVML (dlopen), nvidia-smi fallback
