#include "EarlyUnlockHook.hpp"
#include "SwitchManager.hpp"
#include "Config.hpp"
#include "Logger.hpp"
#include "PatternScan.hpp"
#include <MinHook.h>
#include <iostream>
#include <chrono>

namespace CrimsonSwitch {

bool EarlyUnlockHook::s_isInstalled = false;
WNDPROC EarlyUnlockHook::s_originalWndProc = nullptr;
HWND EarlyUnlockHook::s_gameWindow = nullptr;
HANDLE EarlyUnlockHook::s_hMonitorThread = nullptr;
bool EarlyUnlockHook::s_stopRequested = false;

// Function prototype for FindCompanionById (RVA 0xa7d7f0)
typedef void* (__fastcall *tFindCompanionById)(void* pContainer, uint32_t charId, uint8_t flag);
static tFindCompanionById oFindCompanionById = nullptr;

static void* __fastcall Hook_FindCompanionById(void* pContainer, uint32_t charId, uint8_t flag) {
    void* res = nullptr;
    if (oFindCompanionById) {
        res = oFindCompanionById(pContainer, charId, flag);
    }
    if (!res && (charId == 2 || charId == 3) && pContainer != nullptr) {
        uintptr_t fnActivateAddr = SwitchManager::Instance().GetSelectOrActivateFunction();
        if (fnActivateAddr) {
            auto fnActivate = reinterpret_cast<void* (__fastcall*)(void*, uint32_t)>(fnActivateAddr);
            res = fnActivate(pContainer, charId);
            Logger::Log("[EarlyUnlockHook] Detour: Auto-registered Mercenary ID %u (0x%p) via FindCompanionById query\n", charId, res);
        }
    }
    return res;
}

LRESULT CALLBACK EarlyUnlockHook::Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if ((uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) && !(lParam & (1 << 30))) {
        const auto& cfg = Config::Get();
        if (cfg.enableDirectHotkeys) {
            bool altPressed = ((lParam & (1 << 29)) != 0) || ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);

            bool triggerKliff   = (altPressed && wParam == cfg.keyKliff)   || (wParam == VK_F1) || (wParam == VK_NUMPAD1);
            bool triggerDamiane = (altPressed && wParam == cfg.keyDamiane) || (wParam == VK_F3) || (wParam == VK_NUMPAD2);
            bool triggerOongka  = (altPressed && wParam == cfg.keyOongka)  || (wParam == VK_F4) || (wParam == VK_NUMPAD3);
            bool triggerCycle   = (altPressed && wParam == cfg.keyCycleNext) || (wParam == VK_NUMPAD0);

            std::string msg;
            if (triggerKliff) {
                SwitchManager::Instance().SwitchToCharacter(CharacterType::Kliff, msg);
                Logger::Log("[Hotkey WndProc] Switch to Kliff: %s\n", msg.c_str());
                return 0;
            } else if (triggerDamiane) {
                SwitchManager::Instance().SwitchToCharacter(CharacterType::Damiane, msg);
                Logger::Log("[Hotkey WndProc] Switch to Damiane: %s\n", msg.c_str());
                return 0;
            } else if (triggerOongka) {
                SwitchManager::Instance().SwitchToCharacter(CharacterType::Oongka, msg);
                Logger::Log("[Hotkey WndProc] Switch to Oongka: %s\n", msg.c_str());
                return 0;
            } else if (triggerCycle) {
                SwitchManager::Instance().CycleNextCharacter(msg);
                Logger::Log("[Hotkey WndProc] Cycle: %s\n", msg.c_str());
                return 0;
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

    auto lastHotkeyTime = std::chrono::steady_clock::now();

    while (!s_stopRequested) {
        const auto& cfg = Config::Get();

        // 1. Maintain active game window subclassing (handles window destruction across splash / main menu)
        if (!IsWindow(s_gameWindow)) {
            s_originalWndProc = nullptr;
            s_gameWindow = FindWindowW(nullptr, L"Crimson Desert");
            if (s_gameWindow) {
                DWORD pid = 0;
                GetWindowThreadProcessId(s_gameWindow, &pid);
                if (pid == GetCurrentProcessId()) {
                    s_originalWndProc = (WNDPROC)SetWindowLongPtr(s_gameWindow, GWLP_WNDPROC, (LONG_PTR)Hook_WndProc);
                    if (s_originalWndProc) {
                        Logger::Log("[EarlyUnlockHook] Subclassed active game window: 0x%p (PID %lu)\n", (void*)s_gameWindow, pid);
                    }
                } else {
                    s_gameWindow = nullptr;
                }
            }
        }

        // 2. Maintain unlocked bitmask and populate companion container
        if (cfg.unlockAllFromStart) {
            SwitchManager::Instance().ForceUnlockAll();
        }

        // 3. High-reliability asynchronous hotkey polling with 250ms debouncing
        if (cfg.enableDirectHotkeys) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHotkeyTime).count();

            if (elapsedMs >= 250) {
                bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

                // Support both Alt+1/2/3/C AND direct functional keys F1 / F3 / F4:
                // F1 = Kliff, F3 = Damiane, F4 = Oongka (F2 is HUD Overlay in CrimsonCoop)
                bool triggerKliff = (altPressed && (GetAsyncKeyState(cfg.keyKliff) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_F1) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0);

                bool triggerDamiane = (altPressed && (GetAsyncKeyState(cfg.keyDamiane) & 0x8000) != 0) ||
                                      ((GetAsyncKeyState(VK_F3) & 0x8000) != 0) ||
                                      ((GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0);

                bool triggerOongka = (altPressed && (GetAsyncKeyState(cfg.keyOongka) & 0x8000) != 0) ||
                                     ((GetAsyncKeyState(VK_F4) & 0x8000) != 0) ||
                                     ((GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0);

                bool triggerCycle = (altPressed && (GetAsyncKeyState(cfg.keyCycleNext) & 0x8000) != 0) ||
                                    ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0);

                if (triggerKliff) {
                    lastHotkeyTime = now;
                    std::string msg;
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Kliff, msg);
                    Logger::Log("[Hotkey Async] Switch to Kliff: %s\n", msg.c_str());
                } else if (triggerDamiane) {
                    lastHotkeyTime = now;
                    std::string msg;
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Damiane, msg);
                    Logger::Log("[Hotkey Async] Switch to Damiane: %s\n", msg.c_str());
                } else if (triggerOongka) {
                    lastHotkeyTime = now;
                    std::string msg;
                    SwitchManager::Instance().SwitchToCharacter(CharacterType::Oongka, msg);
                    Logger::Log("[Hotkey Async] Switch to Oongka: %s\n", msg.c_str());
                } else if (triggerCycle) {
                    lastHotkeyTime = now;
                    std::string msg;
                    SwitchManager::Instance().CycleNextCharacter(msg);
                    Logger::Log("[Hotkey Async] Cycle: %s\n", msg.c_str());
                }
            }
        }

        Sleep(50);
    }

    Logger::Log("[EarlyUnlockHook] State monitor thread exited.\n");
    return 0;
}

bool EarlyUnlockHook::Install() {
    if (s_isInstalled) return true;

    Logger::Log("[EarlyUnlockHook] Installing CrimsonSwitch hooks...\n");

    // 1. Initialize SwitchManager
    SwitchManager::Instance().Initialize();

    // 2. Initialize MinHook and hook FindCompanionById
    if (MH_Initialize() == MH_OK || MH_Initialize() == MH_ERROR_ALREADY_INITIALIZED) {
        uintptr_t fnFindAddr = SwitchManager::Instance().GetFindCompanionFunction();
        if (fnFindAddr) {
            if (MH_CreateHook(reinterpret_cast<LPVOID>(fnFindAddr),
                              reinterpret_cast<LPVOID>(&Hook_FindCompanionById),
                              reinterpret_cast<LPVOID*>(&oFindCompanionById)) == MH_OK) {
                MH_EnableHook(reinterpret_cast<LPVOID>(fnFindAddr));
                Logger::Log("[EarlyUnlockHook] Successfully installed detour on FindCompanionById (0x%p)!\n", (void*)fnFindAddr);
            }
        }
    }

    // 3. Launch background state & hotkey thread
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

    uintptr_t fnFindAddr = SwitchManager::Instance().GetFindCompanionFunction();
    if (fnFindAddr) {
        MH_DisableHook(reinterpret_cast<LPVOID>(fnFindAddr));
    }

    s_isInstalled = false;
    Logger::Log("[EarlyUnlockHook] Hooks removed.\n");
}

} // namespace CrimsonSwitch
