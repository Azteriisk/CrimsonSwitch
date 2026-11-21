#include "EarlyUnlockHook.hpp"
#include "SwitchManager.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "PatternScan.hpp"
#include <MinHook.h>
#include <iostream>

namespace CrimsonSwitch {

bool EarlyUnlockHook::s_isInstalled = false;
WNDPROC EarlyUnlockHook::s_originalWndProc = nullptr;
HWND EarlyUnlockHook::s_gameWindow = nullptr;
HANDLE EarlyUnlockHook::s_hMonitorThread = nullptr;
bool EarlyUnlockHook::s_stopRequested = false;

typedef bool(WINAPI* tIsCharacterUnlocked)(void* pUserActor, uint32_t charId);
static tIsCharacterUnlocked oIsCharacterUnlocked = nullptr;

[[maybe_unused]] static bool WINAPI Hook_IsCharacterUnlocked(void* pUserActor, uint32_t charId) {
    // If querying Kliff (1), Damiane (2), or Oongka (3), force unlock
    if (charId >= 1 && charId <= 3) {
        return true;
    }
    if (oIsCharacterUnlocked) {
        return oIsCharacterUnlocked(pUserActor, charId);
    }
    return true;
}

LRESULT CALLBACK EarlyUnlockHook::Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if ((uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) && !(lParam & (1 << 30))) {
        const auto& cfg = Config::Get();
        if (cfg.enableDirectHotkeys) {
            bool modifierActive = true;
            if (cfg.requireModifier && cfg.modifierKey != 0) {
                modifierActive = (GetAsyncKeyState(cfg.modifierKey) & 0x8000) != 0;
            }

            if (modifierActive) {
                std::string msg;
                if (wParam == cfg.keyKliff) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Kliff, msg);
                    Logger::Log("[Hotkey] Switch to Kliff requested: %s\n", msg.c_str());
                    return 0;
                } else if (wParam == cfg.keyDamiane) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Damiane, msg);
                    Logger::Log("[Hotkey] Switch to Damiane requested: %s\n", msg.c_str());
                    return 0;
                } else if (wParam == cfg.keyOongka) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Oongka, msg);
                    Logger::Log("[Hotkey] Switch to Oongka requested: %s\n", msg.c_str());
                    return 0;
                } else if (wParam == cfg.keyCycleNext) {
                    SwitchManager::Instance().CycleNextCharacter(msg);
                    Logger::Log("[Hotkey] Cycle character requested: %s\n", msg.c_str());
                    return 0;
                }
            }
        }
    }

    if (s_originalWndProc) {
        return CallWindowProc(s_originalWndProc, hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI EarlyUnlockHook::MonitorThread(LPVOID lpParam) {
    (void)lpParam;
    Logger::Log("[EarlyUnlockHook] State monitor thread started.\n");

    while (!s_stopRequested) {
        const auto& cfg = Config::Get();

        // 1. Maintain unlocked status on active UserActor
        if (cfg.unlockAllFromStart) {
            SwitchManager::Instance().ForceUnlockAll();
        }

        // 2. Fail-safe GetAsyncKeyState polling for direct hotkeys (only if WndProc is not hooked)
        if (cfg.enableDirectHotkeys && !s_originalWndProc) {
            bool modifierActive = true;
            if (cfg.requireModifier && cfg.modifierKey != 0) {
                modifierActive = (GetAsyncKeyState(cfg.modifierKey) & 0x8000) != 0;
            }

            if (modifierActive) {
                std::string msg;
                if (GetAsyncKeyState(cfg.keyKliff) & 1) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Kliff, msg);
                    Logger::Log("[Hotkey Async] Switch to Kliff: %s\n", msg.c_str());
                } else if (GetAsyncKeyState(cfg.keyDamiane) & 1) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Damiane, msg);
                    Logger::Log("[Hotkey Async] Switch to Damiane: %s\n", msg.c_str());
                } else if (GetAsyncKeyState(cfg.keyOongka) & 1) {
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Oongka, msg);
                    Logger::Log("[Hotkey Async] Switch to Oongka: %s\n", msg.c_str());
                } else if (GetAsyncKeyState(cfg.keyCycleNext) & 1) {
                    SwitchManager::Instance().CycleNextCharacter(msg);
                    Logger::Log("[Hotkey Async] Cycle: %s\n", msg.c_str());
                }
            }
        }

        Sleep(100);
    }

    Logger::Log("[EarlyUnlockHook] State monitor thread exited.\n");
    return 0;
}

bool EarlyUnlockHook::Install() {
    if (s_isInstalled) return true;

    Logger::Log("[EarlyUnlockHook] Installing CrimsonSwitch hooks...\n");

    // Initialize SwitchManager
    SwitchManager::Instance().Initialize();

    // 1. Hook Window Procedure on active game window
    s_gameWindow = FindWindowW(nullptr, L"Crimson Desert");
    if (!s_gameWindow) {
        s_gameWindow = GetActiveWindow();
    }
    if (s_gameWindow && !s_originalWndProc) {
        s_originalWndProc = (WNDPROC)SetWindowLongPtr(s_gameWindow, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
        Logger::Log("[EarlyUnlockHook] Hooked WndProc on game window: 0x%p\n", (void*)s_gameWindow);
    }

    // 2. Launch background state & hotkey thread
    s_stopRequested = false;
    s_hMonitorThread = CreateThread(nullptr, 0, MonitorThread, nullptr, 0, nullptr);

    s_isInstalled = true;
    Logger::Log("[EarlyUnlockHook] Hooks installed successfully. All 3 Greymane heroes unlocked!\n");
    return true;
}

void EarlyUnlockHook::Remove() {
    if (!s_isInstalled) return;

    s_stopRequested = true;
    if (s_hMonitorThread) {
        WaitForSingleObject(s_hMonitorThread, 2000);
        CloseHandle(s_hMonitorThread);
        s_hMonitorThread = nullptr;
    }

    if (s_gameWindow && s_originalWndProc) {
        SetWindowLongPtr(s_gameWindow, GWLP_WNDPROC, (LONG_PTR)s_originalWndProc);
        s_originalWndProc = nullptr;
    }

    s_isInstalled = false;
    Logger::Log("[EarlyUnlockHook] Hooks removed.\n");
}

} // namespace CrimsonSwitch
