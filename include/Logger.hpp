#pragma once

#include <string>

namespace CrimsonSwitch {

class Logger {
public:
    static void Init(const std::string& filename = "CrimsonSwitch.log");
    static void Log(const char* fmt, ...);
    static void Shutdown();

private:
    static std::string s_logPath;
    static std::string s_secondaryPath;
    static bool s_isInitialized;
};

} // namespace CrimsonSwitch
