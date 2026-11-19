#include "disk.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <sys/statvfs.h>

// /proc/mounts escapes spaces etc. as octal (\040). Undo that.
static std::string unescapeMount(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 3 < s.size() + 0 && std::isdigit((unsigned char)s[i+1])) {
            out += static_cast<char>(std::stoi(s.substr(i + 1, 3), nullptr, 8));
            i += 3;
        } else out += s[i];
    }
    return out;
}

std::vector<MountEntry> parseMounts(const std::string& text) {
    std::vector<MountEntry> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        auto f = splitWhitespace(line);
        if (f.size() < 3) continue;
        out.push_back({unescapeMount(f[0]), unescapeMount(f[1]), f[2]});
    }
    return out;
}

bool isRealFilesystem(const MountEntry& m) {
    static const std::set<std::string> real = {
        "ext2", "ext3", "ext4", "btrfs", "xfs", "f2fs", "zfs", "vfat", "exfat",
        "ntfs", "ntfs3", "fuseblk", "reiserfs", "jfs", "nfs", "nfs4", "cifs"};
    if (m.device.rfind("/dev/loop", 0) == 0) return false;
    return real.count(m.fsType) > 0;
}

std::string parentDiskName(const std::string& deviceName) {
    // Maps a partition name to its parent disk:
    //   nvme0n1p2 -> nvme0n1, mmcblk0p1 -> mmcblk0, sda1 -> sda, sda -> sda
    auto endsDigits = [](const std::string& s) {
        return !s.empty() && std::isdigit(static_cast<unsigned char>(s.back()));
    };
    // Families whose disk name itself ends in a digit use a "p<N>" partition suffix.
    bool pStyle = deviceName.rfind("nvme", 0) == 0 || deviceName.rfind("mmcblk", 0) == 0 ||
                  deviceName.rfind("loop", 0) == 0;
    if (pStyle) {
        size_t p = deviceName.rfind('p');
        if (p != std::string::npos && p > 0 && p + 1 < deviceName.size() &&
            std::all_of(deviceName.begin() + p + 1, deviceName.end(),
                        [](unsigned char c) { return std::isdigit(c); }) &&
            endsDigits(deviceName.substr(0, p)))
            return deviceName.substr(0, p);
        return deviceName;
    }
    // sd*, vd*, xvd*, hd*: disk name ends in letters, partition is trailing digits.
    size_t end = deviceName.size();
    while (end > 0 && std::isdigit(static_cast<unsigned char>(deviceName[end - 1]))) --end;
    return end == 0 ? deviceName : deviceName.substr(0, end);
}

static std::optional<bool> detectSsd(const std::string& device) {
    if (device.rfind("/dev/", 0) != 0) return std::nullopt;
    std::string parent = parentDiskName(device.substr(5));
    auto rot = readFirstLineAsInt("/sys/block/" + parent + "/queue/rotational");
    if (!rot) return std::nullopt;
    return *rot == 0;
}

std::vector<DiskInfo> collectDisks() {
    std::vector<DiskInfo> out;
    std::set<std::string> seen;
    for (const auto& m : parseMounts(readFile("/proc/mounts").value_or(""))) {
        if (!isRealFilesystem(m)) continue;
        if (!seen.insert(m.device).second) continue;   // keep first mount point only
        struct statvfs s{};
        if (statvfs(m.mountPoint.c_str(), &s) != 0) continue;
        DiskInfo d;
        d.device = m.device; d.mountPoint = m.mountPoint; d.fsType = m.fsType;
        std::uint64_t fr = s.f_frsize;
        d.total = s.f_blocks * fr;
        d.used = (s.f_blocks - s.f_bfree) * fr;      // same definition as df
        d.available = s.f_bavail * fr;
        d.isSsd = detectSsd(m.device);
        out.push_back(std::move(d));
    }
    return out;
}
