#pragma once
#include <cstdint>
#include <string>

std::string formatBytes(std::uint64_t bytes);
std::string progressBar(double percent, int width = 20, bool color = false);
