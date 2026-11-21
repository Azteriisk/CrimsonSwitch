#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace CrimsonSwitch {

struct PatternByte {
    uint8_t value{0};
    bool isWildcard{false};
};

class PatternScanner {
public:
    static std::vector<PatternByte> ParsePattern(const std::string& patternStr);
    static uintptr_t Scan(HMODULE hModule, const std::string& patternStr);
    static uintptr_t ResolveRipRelative(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength);
};

} // namespace CrimsonSwitch
