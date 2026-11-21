#include <windows.h>
#include <iostream>
#include "Logger.hpp"
#include "Config.hpp"
#include "EarlyUnlockHook.hpp"
#include "SwitchManager.hpp"

namespace CrimsonSwitch {

DWORD WINAPI ModMainThread(LPVOID lpParam) {
    (void)lpParam;

    Logger::Init("CrimsonSwitch.log");
    Logger::Log("=========================================================\n");
    Logger::Log("  Crimson Desert Early Trio Mercenary Unlock Mod Loaded  \n");
    Logger::Log("  Instant Switching: Kliff, Damiane, Oongka              \n");
    Logger::Log("=========================================================\n");

    Config::Load();
    const auto& cfg = Config::Get();

    if (cfg.allocateConsole) {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        Logger::Log("[Core] Developer console allocated.\n");
    }

    if (EarlyUnlockHook::Install()) {
        Logger::Log("[Core] EarlyUnlockHook active: 3-character switching enabled.\n");
    } else {
        Logger::Log("[Core] Error: Failed to initialize EarlyUnlockHook!\n");
    }

    return 0;
}

} // namespace CrimsonSwitch

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, CrimsonSwitch::ModMainThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        CrimsonSwitch::EarlyUnlockHook::Remove();
        CrimsonSwitch::Logger::Shutdown();
        break;
    }
    return TRUE;
}
