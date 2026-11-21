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
    bool requireModifier{true};   // Requires modifier key so normal 1/2/3/Tab gameplay actions are never stolen
    uint32_t modifierKey{0x12};   // VK_MENU (ALT = 0x12, CTRL = 0x11, SHIFT = 0x10)
    uint32_t keyKliff{0x31};      // '1' Key (Alt + 1)
    uint32_t keyDamiane{0x32};    // '2' Key (Alt + 2)
    uint32_t keyOongka{0x33};     // '3' Key (Alt + 3)
    uint32_t keyCycleNext{0x43};  // 'C' Key (Alt + C) avoids Windows Alt+Tab conflict

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
