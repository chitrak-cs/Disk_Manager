# System Monitor in C++ — Project Plan

A command-line C++ program that takes a **snapshot** of the machine at one moment
and prints:

1. **Disk storage** — every mounted drive/partition: total, used, free, % used, filesystem type
2. **RAM usage** — total, used, available, buffers/cache, swap
3. **GPU usage** — per GPU: utilization %, VRAM used/total, temperature (and power/clock where available)

Later it can grow a "live" mode that refreshes every N seconds.

---

## 0. Target machine (detected on 2026-11-19)

| Item | Value | What it means for the code |
|---|---|---|
| OS | Linux (Ubuntu 24.04, kernel 7.0) | Use `/proc`, `/sys`, `statvfs` — no Windows APIs needed |
| Compiler | g++ 13.3, clang++, cmake | C++20 is available |
| Disk | `nvme0n1` 476.9 GB + many `loop` devices (snaps) | Must **filter out** `loop`, `tmpfs`, `squashfs`, etc. |
| GPU 1 | NVIDIA GTX 1650 Mobile (`card1`, vendor `0x10de`) | Use **NVML** (`libnvidia-ml.so.1` is installed; `nvml.h` is **not**) |
| GPU 2 | AMD Radeon Vega 6 iGPU (`card2`, vendor `0x1002`) | Read **sysfs**: `gpu_busy_percent`, `mem_info_vram_*` |

This is a hybrid-graphics laptop, so the program must handle **two different GPU
vendors** at once — that drives the design below.

---

## 1. Where each number comes from (Linux data sources)

### Disk
| Data | Source |
|---|---|
| List of mounted filesystems | `/proc/mounts` (fields: device, mount point, fs type, …) |
| Total / free / available bytes | `statvfs(mount_point)` → `f_blocks`, `f_bfree`, `f_bavail` × `f_frsize` |
| Physical disk model / size | `/sys/block/<dev>/device/model`, `/sys/block/<dev>/size` (× 512) |
| SSD or HDD | `/sys/block/<dev>/queue/rotational` (0 = SSD) |

> Note: `f_bfree` includes blocks reserved for root; `f_bavail` is what a normal user can use. `df` shows `used = blocks - bfree`, `avail = bavail`.

### RAM
| Data | Source |
|---|---|
| MemTotal, MemFree, MemAvailable, Buffers, Cached, SwapTotal, SwapFree | `/proc/meminfo` (values in kB) |

> "Used" RAM should be `MemTotal - MemAvailable`, **not** `MemTotal - MemFree` (Linux uses free RAM as cache).

### GPU
| Vendor | Data | Source |
|---|---|---|
| NVIDIA | utilization, VRAM, temp, power, name | NVML: `nvmlDeviceGetUtilizationRates`, `nvmlDeviceGetMemoryInfo`, `nvmlDeviceGetTemperature`, `nvmlDeviceGetPowerUsage`, `nvmlDeviceGetName` |
| AMD | utilization % | `/sys/class/drm/cardN/device/gpu_busy_percent` |
| AMD | VRAM used / total | `/sys/class/drm/cardN/device/mem_info_vram_used`, `mem_info_vram_total` |
| AMD | temperature | `/sys/class/drm/cardN/device/hwmon/hwmon*/temp1_input` (millidegrees C) |
| Any | which vendor a card is | `/sys/class/drm/cardN/device/vendor` (`0x10de` NVIDIA, `0x1002` AMD, `0x8086` Intel) |
| Fallback | NVIDIA without NVML | `popen("nvidia-smi --query-gpu=... --format=csv,noheader,nounits")` |

> Since `nvml.h` is missing, load NVML at runtime with `dlopen("libnvidia-ml.so.1")` +
> `dlsym`, and declare the few function signatures/structs we need ourselves.
> Bonus: the program still runs on machines **without** an NVIDIA driver.

---

## 2. Project layout

```
Disk Management/
├── PLAN.md                 ← this file
├── CMakeLists.txt
├── include/
│   ├── types.hpp           ← DiskInfo, MemoryInfo, GpuInfo structs
│   ├── disk.hpp
│   ├── memory.hpp
│   ├── gpu.hpp
│   ├── format.hpp          ← byte → "12.4 GiB", progress bars
│   └── utils.hpp           ← readFile(), trim(), split()
├── src/
│   ├── main.cpp            ← arg parsing, calls collectors, prints report
│   ├── disk.cpp
│   ├── memory.cpp
│   ├── gpu_nvidia.cpp      ← NVML via dlopen
│   ├── gpu_amd.cpp         ← sysfs reader
│   ├── gpu.cpp             ← enumerates cards, dispatches by vendor
│   ├── format.cpp
│   └── utils.cpp
└── tests/
    └── test_parsers.cpp    ← parse sample /proc/meminfo, /proc/mounts text
```

**Core design idea:** each collector is a function that returns plain data
(`std::vector<DiskInfo> collectDisks()`, `MemoryInfo collectMemory()`,
`std::vector<GpuInfo> collectGpus()`). Printing is completely separate. That way
the same data can later be printed as a table, as JSON, or refreshed in a loop.

---

## 3. Steps, broken into chunks

Each step ends with something you can **compile and run**.

### Step 1 — Project skeleton  *(~30 min)*
- [ ] Create folders and `CMakeLists.txt` (C++20, `-Wall -Wextra`, link `dl`)
- [ ] `main.cpp` prints "System Monitor v0.1"
- [ ] Build: `cmake -B build && cmake --build build && ./build/sysmon`
- **Done when:** the binary builds and runs.

### Step 2 — Shared utilities  *(~45 min)*
- [ ] `readFile(path) -> std::optional<std::string>` (returns `nullopt` if missing/unreadable)
- [ ] `readFirstLineAsInt(path) -> std::optional<long long>` (for sysfs single-value files)
- [ ] `trim`, `split` helpers
- [ ] `formatBytes(uint64_t) -> "476.9 GiB"` (binary units, 1 decimal)
- [ ] `progressBar(double percent, int width) -> "[#######---]"`
- **Done when:** a quick test prints `formatBytes(1536) == "1.5 KiB"`.

### Step 3 — Data structures  *(~20 min)*
- [ ] `struct DiskInfo { device, mountPoint, fsType; uint64_t total, used, available; bool isSsd; }`
- [ ] `struct MemoryInfo { uint64_t total, used, available, buffers, cached, swapTotal, swapUsed; }`
- [ ] `struct GpuInfo { name, vendor; std::optional<int> utilPercent, tempC; std::optional<uint64_t> vramUsed, vramTotal; std::optional<double> powerW; }`
- Use `std::optional` for GPU fields — not every GPU reports every metric.

### Step 4 — RAM collector  *(easiest real one, ~45 min)*
- [ ] Parse `/proc/meminfo` line by line into a `std::map<std::string, uint64_t>` (kB → bytes)
- [ ] Fill `MemoryInfo`; `used = MemTotal - MemAvailable`
- [ ] Print RAM + swap with a progress bar
- **Verify:** compare with `free -b`.

### Step 5 — Disk collector  *(~1.5 h)*
- [ ] Read `/proc/mounts`, split each line into device / mount point / fs type
- [ ] **Filter** pseudo filesystems: skip `proc, sysfs, tmpfs, devtmpfs, squashfs, overlay, cgroup*, devpts, securityfs, …` and `/dev/loop*`
- [ ] De-duplicate devices mounted twice (keep first mount point)
- [ ] Call `statvfs()` per mount point; compute total / used / available
- [ ] Look up `/sys/block/<dev>/queue/rotational` for SSD/HDD (strip partition suffix: `nvme0n1p2 → nvme0n1`, `sda1 → sda`)
- [ ] Print a table: Device | Mount | Type | Size | Used | Free | Use%
- **Verify:** compare with `df -hT -x tmpfs -x squashfs`.

### Step 6 — GPU discovery  *(~45 min)*
- [ ] Iterate `/sys/class/drm/card[0-9]*` (skip `card1-DP-1` style connector entries)
- [ ] Read `device/vendor` to classify NVIDIA / AMD / Intel / unknown
- [ ] Dispatch to the matching vendor reader
- **Done when:** program lists "card1: NVIDIA", "card2: AMD".

### Step 7 — AMD GPU reader (sysfs)  *(~45 min)*
- [ ] `gpu_busy_percent` → utilization
- [ ] `mem_info_vram_used` / `mem_info_vram_total` → VRAM
- [ ] First `hwmon/hwmon*/temp1_input` ÷ 1000 → °C
- [ ] Name: from `/sys/class/drm/cardN/device/product_name` if present, else "AMD GPU (PCI id)"
- **Verify:** `cat /sys/class/drm/card2/device/gpu_busy_percent` matches.

### Step 8 — NVIDIA GPU reader (NVML via dlopen)  *(hardest, ~2 h)*
- [ ] `dlopen("libnvidia-ml.so.1", RTLD_LAZY)`; if it fails → try the `nvidia-smi` fallback → else mark "driver not available"
- [ ] Declare minimal types: `nvmlReturn_t`, `nvmlDevice_t`, `nvmlUtilization_t {unsigned gpu, memory;}`, `nvmlMemory_t {unsigned long long total, free, used;}`
- [ ] `dlsym` the functions: `nvmlInit_v2`, `nvmlShutdown`, `nvmlDeviceGetCount_v2`, `nvmlDeviceGetHandleByIndex_v2`, `nvmlDeviceGetName`, `nvmlDeviceGetUtilizationRates`, `nvmlDeviceGetMemoryInfo`, `nvmlDeviceGetTemperature`, `nvmlDeviceGetPowerUsage`
- [ ] Wrap init/shutdown in an RAII class so `nvmlShutdown` + `dlclose` always run
- [ ] Treat each call's failure independently (leave that `optional` empty)
- **Verify:** compare with `nvidia-smi`.
- ⚠ On hybrid laptops the NVIDIA GPU may be in a **sleep (D3cold) state**; querying it can wake it up. Note this in output or add a `--no-wake` flag later.

### Step 9 — Report output  *(~1 h)*
- [ ] Header: hostname (`gethostname`), timestamp (`std::chrono` + `std::put_time`)
- [ ] Three sections: STORAGE / MEMORY / GPU, aligned columns (`std::setw`)
- [ ] Colour the progress bars by load (green < 60%, yellow < 85%, red ≥ 85%) using ANSI codes; disable when `!isatty(STDOUT_FILENO)`
- [ ] Show "N/A" for empty optionals

### Step 10 — Command-line options  *(~45 min)*
- [ ] `--disk`, `--ram`, `--gpu` (show only one section)
- [ ] `--watch N` — clear screen and refresh every N seconds
- [ ] `--json` — machine-readable output
- [ ] `--help`

### Step 11 — Testing & hardening  *(~1 h)*
- [ ] Unit-test the parsers with **sample text** (don't depend on the real `/proc`)
- [ ] Run as a normal user (some sysfs files may be root-only — must not crash)
- [ ] Run with the NVIDIA driver missing (rename lib path via `LD_LIBRARY_PATH` trick or test on another machine)
- [ ] Check with `valgrind` / `-fsanitize=address,undefined`

### Step 12 — Stretch goals (optional)
- [ ] Per-process GPU memory (`nvmlDeviceGetComputeRunningProcesses`)
- [ ] Disk I/O speed: read `/proc/diskstats` twice 1 s apart and diff
- [ ] CPU usage: `/proc/stat` twice and diff
- [ ] Disk health via SMART (`smartctl` / NVMe `/sys/class/nvme/nvme0/`)
- [ ] TUI with `ncurses` or `FTXUI`
- [ ] Windows port (`GlobalMemoryStatusEx`, `GetDiskFreeSpaceEx`, DXGI/NVML) behind `#ifdef _WIN32`

---

## 4. Suggested order & milestones

| Milestone | Steps | Result |
|---|---|---|
| M1 — "It builds" | 1–3 | Skeleton + helpers + structs |
| M2 — "RAM & Disk" | 4–5 | First useful output, verifiable against `free` / `df` |
| M3 — "GPUs" | 6–8 | Both GPUs reported |
| M4 — "Polished" | 9–11 | Nice report, flags, tests |
| M5 — "Extras" | 12 | Whatever you find interesting |

---

## 5. Pieces worth writing yourself (learning focus)

These are the spots with real design decisions — good places for you to write
the code while I scaffold the rest:

1. **Filesystem filter (Step 5)** — which mounts count as "real storage"? Blacklist of fs types vs. whitelist (`ext4, btrfs, xfs, vfat, ntfs, …`)?
2. **Partition → parent disk name (Step 5)** — `nvme0n1p2 → nvme0n1`, `sda1 → sda`, `mmcblk0p1 → mmcblk0`.
3. **NVML error handling (Step 8)** — what to do when init succeeds but one metric fails.
4. **Colour thresholds & layout (Step 9)** — what the report looks like is your call.

---

## 6. Build & run cheat-sheet

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/sysmon
./build/sysmon --watch 2
```

Cross-check commands: `df -hT`, `free -h`, `nvidia-smi`, `lsblk`,
`cat /sys/class/drm/card2/device/gpu_busy_percent`.
