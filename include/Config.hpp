#pragma once

#include "Common.hpp"
#include <string>
#include <cstdint>

namespace CrimsonSwitch {

struct SwitchConfig {
    // [General]
    bool unlockAllFromStart{true};
    bool enableDirectHotkeys{true};
    bool spawnMissingCompanions{true};

    // [Hotkeys]
    uint32_t keyKliff{0x31};     // '1' Key
    uint32_t keyDamiane{0x32};   // '2' Key
    uint32_t keyOongka{0x33};    // '3' Key
    uint32_t keyCycleNext{0x09}; // TAB Key

    // [Advanced]
    bool debugLogging{true};
    bool allocateConsole{false};
};

class Config {
public:
    static bool Load(const std::string& configPath = "CrimsonSwitch.ini");
    static bool Save(const std::string& configPath = "CrimsonSwitch.ini");

    static const SwitchConfig& Get() { return s_config; }
    static void Set(const SwitchConfig& config) { s_config = config; }

private:
    static SwitchConfig s_config;
    static std::string s_configPath;
};

} // namespace CrimsonSwitch
