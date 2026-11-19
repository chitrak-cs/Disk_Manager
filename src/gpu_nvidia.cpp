#include "gpu.hpp"
#include "utils.hpp"
#include <cstdio>
#include <dlfcn.h>

namespace {
// Minimal NVML ABI declarations (nvml.h is not installed).
using nvmlReturn_t = int;
using nvmlDevice_t = void*;
struct nvmlUtilization_t { unsigned gpu, memory; };
struct nvmlMemory_t { unsigned long long total, free, used; };
constexpr nvmlReturn_t NVML_SUCCESS = 0;
constexpr int NVML_TEMPERATURE_GPU = 0;

// RAII: dlopen + nvmlInit on construction; nvmlShutdown + dlclose always run.
class Nvml {
public:
    Nvml() {
        lib_ = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
        if (!lib_) return;
        init_ = sym<nvmlReturn_t (*)()>("nvmlInit_v2");
        shutdown_ = sym<nvmlReturn_t (*)()>("nvmlShutdown");
        count = sym<nvmlReturn_t (*)(unsigned*)>("nvmlDeviceGetCount_v2");
        handle = sym<nvmlReturn_t (*)(unsigned, nvmlDevice_t*)>("nvmlDeviceGetHandleByIndex_v2");
        name = sym<nvmlReturn_t (*)(nvmlDevice_t, char*, unsigned)>("nvmlDeviceGetName");
        util = sym<nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*)>("nvmlDeviceGetUtilizationRates");
        mem = sym<nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*)>("nvmlDeviceGetMemoryInfo");
        temp = sym<nvmlReturn_t (*)(nvmlDevice_t, int, unsigned*)>("nvmlDeviceGetTemperature");
        power = sym<nvmlReturn_t (*)(nvmlDevice_t, unsigned*)>("nvmlDeviceGetPowerUsage");
        ok_ = init_ && shutdown_ && count && handle && init_() == NVML_SUCCESS;
    }
    ~Nvml() {
        if (ok_) shutdown_();
        if (lib_) dlclose(lib_);
    }
    Nvml(const Nvml&) = delete;
    Nvml& operator=(const Nvml&) = delete;
    bool ok() const { return ok_; }

    nvmlReturn_t (*count)(unsigned*) = nullptr;
    nvmlReturn_t (*handle)(unsigned, nvmlDevice_t*) = nullptr;
    nvmlReturn_t (*name)(nvmlDevice_t, char*, unsigned) = nullptr;
    nvmlReturn_t (*util)(nvmlDevice_t, nvmlUtilization_t*) = nullptr;
    nvmlReturn_t (*mem)(nvmlDevice_t, nvmlMemory_t*) = nullptr;
    nvmlReturn_t (*temp)(nvmlDevice_t, int, unsigned*) = nullptr;
    nvmlReturn_t (*power)(nvmlDevice_t, unsigned*) = nullptr;

private:
    template <class F> F sym(const char* n) { return reinterpret_cast<F>(dlsym(lib_, n)); }
    void* lib_ = nullptr;
    nvmlReturn_t (*init_)() = nullptr;
    nvmlReturn_t (*shutdown_)() = nullptr;
    bool ok_ = false;
};

std::vector<GpuInfo> viaNvidiaSmi() {
    std::vector<GpuInfo> out;
    FILE* p = popen("nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total,"
                    "temperature.gpu,power.draw --format=csv,noheader,nounits 2>/dev/null", "r");
    if (!p) return out;
    char buf[512];
    int idx = 0;
    while (std::fgets(buf, sizeof buf, p)) {
        auto f = split(buf, ',');
        if (f.size() < 6) continue;
        GpuInfo g; g.vendor = "NVIDIA"; g.id = "nvidia" + std::to_string(idx++);
        g.name = trim(f[0]);
        try {
            g.utilPercent = std::stoi(f[1]);
            g.vramUsed = std::stoull(f[2]) * 1024 * 1024;
            g.vramTotal = std::stoull(f[3]) * 1024 * 1024;
            g.tempC = std::stoi(f[4]);
            g.powerW = std::stod(f[5]);
        } catch (...) {}   // "[N/A]" fields just stay empty
        out.push_back(std::move(g));
    }
    pclose(p);
    return out;
}
}  // namespace

std::vector<GpuInfo> readNvidiaGpus() {
    std::vector<GpuInfo> out;
    {
        Nvml nv;
        unsigned n = 0;
        if (nv.ok() && nv.count(&n) == NVML_SUCCESS) {
            for (unsigned i = 0; i < n; ++i) {
                nvmlDevice_t dev;
                if (nv.handle(i, &dev) != NVML_SUCCESS) continue;
                GpuInfo g; g.vendor = "NVIDIA"; g.id = "nvidia" + std::to_string(i);
                char nm[96] = {};
                g.name = (nv.name && nv.name(dev, nm, sizeof nm) == NVML_SUCCESS) ? nm : "NVIDIA GPU";
                // each metric fails independently -> its optional stays empty
                nvmlUtilization_t u;
                if (nv.util && nv.util(dev, &u) == NVML_SUCCESS) g.utilPercent = static_cast<int>(u.gpu);
                nvmlMemory_t m;
                if (nv.mem && nv.mem(dev, &m) == NVML_SUCCESS) { g.vramUsed = m.used; g.vramTotal = m.total; }
                unsigned t;
                if (nv.temp && nv.temp(dev, NVML_TEMPERATURE_GPU, &t) == NVML_SUCCESS) g.tempC = static_cast<int>(t);
                unsigned mw;
                if (nv.power && nv.power(dev, &mw) == NVML_SUCCESS) g.powerW = mw / 1000.0;
                out.push_back(std::move(g));
            }
            return out;
        }
    }
    out = viaNvidiaSmi();
    if (out.empty()) {
        GpuInfo g; g.vendor = "NVIDIA"; g.id = "nvidia"; g.name = "NVIDIA GPU";
        g.note = "driver not available";
        out.push_back(g);
    }
    return out;
}
