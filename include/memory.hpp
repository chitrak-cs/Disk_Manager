#pragma once
#include "types.hpp"
#include <string>
MemoryInfo parseMeminfo(const std::string& text);   // pure: testable with sample text
MemoryInfo collectMemory();
