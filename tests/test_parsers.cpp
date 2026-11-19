#include "disk.hpp"
#include "format.hpp"
#include "memory.hpp"
#include <cstdlib>
#include <iostream>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL line " << __LINE__ << ": " #c "\n"; ++failures; } } while (0)

int main() {
    CHECK(formatBytes(1536) == "1.5 KiB");
    CHECK(formatBytes(512) == "512 B");

    auto m = parseMeminfo("MemTotal: 1000 kB\nMemFree: 100 kB\nMemAvailable: 400 kB\n"
                          "Buffers: 10 kB\nCached: 20 kB\nSwapTotal: 50 kB\nSwapFree: 30 kB\n");
    CHECK(m.total == 1000 * 1024);
    CHECK(m.used == 600 * 1024);          // total - available, not total - free
    CHECK(m.swapUsed == 20 * 1024);

    auto mounts = parseMounts("/dev/nvme0n1p2 / ext4 rw 0 0\n/dev/loop3 /snap/x squashfs ro 0 0\n"
                              "tmpfs /run tmpfs rw 0 0\n/dev/sdb1 /mnt/my\\040usb vfat rw 0 0\n");
    CHECK(mounts.size() == 4);
    CHECK(mounts[3].mountPoint == "/mnt/my usb");
    CHECK(isRealFilesystem(mounts[0]));
    CHECK(!isRealFilesystem(mounts[1]));
    CHECK(!isRealFilesystem(mounts[2]));

    CHECK(parentDiskName("nvme0n1p2") == "nvme0n1");
    CHECK(parentDiskName("mmcblk0p1") == "mmcblk0");
    CHECK(parentDiskName("sda1") == "sda");
    CHECK(parentDiskName("sda") == "sda");
    CHECK(parentDiskName("nvme0n1") == "nvme0n1");

    std::cout << (failures ? "FAILED\n" : "all tests passed\n");
    return failures ? 1 : 0;
}
