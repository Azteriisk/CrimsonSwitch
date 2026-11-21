#include "PatternScan.hpp"
#include <sstream>
#include <iomanip>

namespace CrimsonSwitch {

std::vector<PatternByte> PatternScanner::ParsePattern(const std::string& patternStr) {
    std::vector<PatternByte> pattern;
    std::istringstream stream(patternStr);
    std::string byteStr;

    while (stream >> byteStr) {
        if (byteStr == "?" || byteStr == "??" || byteStr == "???" || byteStr[0] == '?') {
            pattern.push_back({0, true});
        } else {
            uint8_t value = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            pattern.push_back({value, false});
        }
    }
    return pattern;
}

uintptr_t PatternScanner::Scan(HMODULE hModule, const std::string& patternStr) {
    if (!hModule) {
        hModule = GetModuleHandleW(nullptr);
    }
    if (!hModule) return 0;

    auto pattern = ParsePattern(patternStr);
    if (pattern.empty()) return 0;

    auto* dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return 0;

    auto* ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<uint8_t*>(hModule) + dosHeader->e_lfanew
    );
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return 0;

    uint8_t* base = reinterpret_cast<uint8_t*>(hModule);
    size_t size = ntHeaders->OptionalHeader.SizeOfImage;

    size_t patternSize = pattern.size();
    for (size_t i = 0; i < size - patternSize; ++i) {
        bool match = true;
        for (size_t j = 0; j < patternSize; ++j) {
            if (!pattern[j].isWildcard && pattern[j].value != base[i + j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return reinterpret_cast<uintptr_t>(base + i);
        }
    }
    return 0;
}

uintptr_t PatternScanner::ResolveRipRelative(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength) {
    if (instructionAddress == 0) return 0;
    int32_t displacement = *reinterpret_cast<const int32_t*>(instructionAddress + dispOffset);
    return instructionAddress + instructionLength + displacement;
}

} // namespace CrimsonSwitch
