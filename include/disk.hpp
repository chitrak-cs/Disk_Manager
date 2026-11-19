#pragma once
#include "types.hpp"
#include <string>
#include <vector>

struct MountEntry { std::string device, mountPoint, fsType; };

std::vector<MountEntry> parseMounts(const std::string& text);   // pure
bool isRealFilesystem(const MountEntry& m);
std::string parentDiskName(const std::string& deviceName);       // "nvme0n1p2" -> "nvme0n1"
std::vector<DiskInfo> collectDisks();
