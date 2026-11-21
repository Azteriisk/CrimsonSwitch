#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace CrimsonSwitch {

SwitchConfig Config::s_config{};
std::string Config::s_configPath = "CrimsonSwitch.ini";

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Config::Load(const std::string& configPath) {
    s_configPath = configPath;
    std::ifstream file(configPath);
    if (!file.is_open()) {
        Save(configPath);
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = Trim(line.substr(0, eqPos));
            std::string val = Trim(line.substr(eqPos + 1));

            if (currentSection == "General") {
                if (key == "UnlockAllFromStart")    s_config.unlockAllFromStart = (val == "true" || val == "1");
                if (key == "EnableDirectHotkeys")   s_config.enableDirectHotkeys = (val == "true" || val == "1");
                if (key == "SpawnMissingCompanions")s_config.spawnMissingCompanions = (val == "true" || val == "1");
            } else if (currentSection == "Hotkeys") {
                if (key == "RequireModifier")  s_config.requireModifier = (val == "true" || val == "1");
                if (key == "ModifierKey")      s_config.modifierKey = std::stoul(val, nullptr, 0);
                if (key == "KeyKliff")         s_config.keyKliff = std::stoul(val, nullptr, 0);
                if (key == "KeyDamiane")       s_config.keyDamiane = std::stoul(val, nullptr, 0);
                if (key == "KeyOongka")        s_config.keyOongka = std::stoul(val, nullptr, 0);
                if (key == "KeyCycleNext")     s_config.keyCycleNext = std::stoul(val, nullptr, 0);
            } else if (currentSection == "Advanced") {
                if (key == "DebugLogging")    s_config.debugLogging = (val == "true" || val == "1");
                if (key == "AllocateConsole") s_config.allocateConsole = (val == "true" || val == "1");
            }
        }
    }

    return true;
}

bool Config::Save(const std::string& configPath) {
    std::ofstream file(configPath);
    if (!file.is_open()) return false;

    file << "; =========================================================\n";
    file << "; Crimson Desert Early Trio Mercenary Switch Configuration\n";
    file << "; Unlocks Kliff, Damiane, and Oongka from the Prologue\n";
    file << "; =========================================================\n\n";

    file << "[General]\n";
    file << "UnlockAllFromStart = " << (s_config.unlockAllFromStart ? "true" : "false") << "\n";
    file << "EnableDirectHotkeys = " << (s_config.enableDirectHotkeys ? "true" : "false") << "\n";
    file << "SpawnMissingCompanions = " << (s_config.spawnMissingCompanions ? "true" : "false") << "\n\n";

    file << "[Hotkeys]\n";
    file << "; RequireModifier prevents colliding with standard gameplay actions (1/2/3 items, Tab lock-on)\n";
    file << "; ModifierKey: 0x12 = ALT, 0x11 = CTRL, 0x10 = SHIFT, 0x00 = None\n";
    file << "RequireModifier = " << (s_config.requireModifier ? "true" : "false") << "\n";
    file << "ModifierKey = 0x" << std::hex << s_config.modifierKey << std::dec << "\n";
    file << "; Key codes: 0x31 = '1', 0x32 = '2', 0x33 = '3', 0x43 = 'C'\n";
    file << "KeyKliff = 0x" << std::hex << s_config.keyKliff << std::dec << "\n";
    file << "KeyDamiane = 0x" << std::hex << s_config.keyDamiane << std::dec << "\n";
    file << "KeyOongka = 0x" << std::hex << s_config.keyOongka << std::dec << "\n";
    file << "KeyCycleNext = 0x" << std::hex << s_config.keyCycleNext << std::dec << "\n\n";

    file << "[Advanced]\n";
    file << "DebugLogging = " << (s_config.debugLogging ? "true" : "false") << "\n";
    file << "AllocateConsole = " << (s_config.allocateConsole ? "true" : "false") << "\n";

    return true;
}

} // namespace CrimsonSwitch
