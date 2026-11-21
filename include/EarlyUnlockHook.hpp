#pragma once

#include <windows.h>
#include <string>

namespace CrimsonSwitch {

class EarlyUnlockHook {
public:
    static bool Install();
    static void Remove();

    // Background thread loop for hotkey detection and generation change monitoring
    static DWORD WINAPI MonitorThread(LPVOID lpParam);

    // Window Procedure Hook for seamless direct hotkey inputs
    static LRESULT CALLBACK Hook_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    static bool s_isInstalled;
    static WNDPROC s_originalWndProc;
    static HWND s_gameWindow;
    static HANDLE s_hMonitorThread;
    static bool s_stopRequested;
};

} // namespace CrimsonSwitch
