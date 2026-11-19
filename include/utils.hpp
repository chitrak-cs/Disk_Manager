#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

std::optional<std::string> readFile(const std::string& path);
std::optional<long long> readFirstLineAsInt(const std::string& path);
std::string trim(std::string_view s);
std::vector<std::string> split(std::string_view s, char delim);
std::vector<std::string> splitWhitespace(std::string_view s);
