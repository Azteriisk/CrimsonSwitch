#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace SwitchLauncher {

const std::wstring GAME_EXE_NAME = L"CrimsonDesert.exe";
const std::wstring MOD_DLL_NAME  = L"CrimsonSwitch.dll";

std::wstring CleanPath(std::wstring path) {
    if (path.empty()) return path;
    if (path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.length() - 2);
    }
    return path;
}

DWORD FindProcessId(const std::wstring& processName) {
    DWORD processId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(pe32);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
                    processId = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
    return processId;
}

std::vector<fs::path> GetSteamLibraryFolders() {
    std::vector<fs::path> libraries;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t steamPath[MAX_PATH] = {};
        DWORD pathLen = sizeof(steamPath);
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steamPath), &pathLen) == ERROR_SUCCESS) {
            fs::path mainSteam(steamPath);
            libraries.push_back(mainSteam);

            fs::path vdfPath = mainSteam / L"steamapps" / L"libraryfolders.vdf";
            if (fs::exists(vdfPath)) {
                std::ifstream vdfFile(vdfPath);
                std::string line;
                while (std::getline(vdfFile, line)) {
                    size_t pathKey = line.find("\"path\"");
                    if (pathKey != std::string::npos) {
                        size_t firstQuote = line.find('"', pathKey + 6);
                        size_t secondQuote = line.find('"', firstQuote + 1);
                        if (firstQuote != std::string::npos && secondQuote != std::string::npos) {
                            std::string rawPath = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                            std::string cleanPathStr;
                            for (size_t i = 0; i < rawPath.length(); ++i) {
                                if (rawPath[i] == '\\' && i + 1 < rawPath.length() && rawPath[i + 1] == '\\') {
                                    continue;
                                }
                                cleanPathStr += rawPath[i];
                            }
                            fs::path libPath(cleanPathStr);
                            if (fs::exists(libPath) && std::find(libraries.begin(), libraries.end(), libPath) == libraries.end()) {
                                libraries.push_back(libPath);
                            }
                        }
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }

    return libraries;
}

std::wstring FindGameExecutable() {
    std::vector<fs::path> directChecks = {
        L"D:\\SteamLibrary\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe",
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe",
        L"E:\\SteamLibrary\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe"
    };
    for (const auto& path : directChecks) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
    }

    fs::path current = fs::current_path();
    std::vector<fs::path> localSearch = {
        current / GAME_EXE_NAME,
        current / L"bin64" / GAME_EXE_NAME,
        current.parent_path() / GAME_EXE_NAME,
        current.parent_path() / L"bin64" / GAME_EXE_NAME
    };

    for (const auto& path : localSearch) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
    }

    auto libraries = GetSteamLibraryFolders();
    for (const auto& lib : libraries) {
        fs::path candidate = lib / L"steamapps" / L"common" / L"Crimson Desert" / L"bin64" / GAME_EXE_NAME;
        if (fs::exists(candidate)) {
            return fs::canonical(candidate).wstring();
        }
    }

    return L"";
}

std::wstring FindModDll() {
    fs::path current = fs::current_path();
    std::vector<fs::path> dllSearchPaths = {
        current / MOD_DLL_NAME,
        current / L"build" / MOD_DLL_NAME,
        current / L"build" / L"Release" / MOD_DLL_NAME,
        current.parent_path() / MOD_DLL_NAME,
        current.parent_path() / L"build" / MOD_DLL_NAME
    };

    for (const auto& path : dllSearchPaths) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
    }
    return L"";
}

// Acquire SeDebugPrivilege to ensure memory operations succeed across integrity levels
bool EnableDebugPrivilege() {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
        CloseHandle(hToken);
        return false;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
    CloseHandle(hToken);
    return true;
}

bool InjectDll(HANDLE hProcess, const std::wstring& dllPath) {
    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);

    LPVOID pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        DWORD err = GetLastError();
        std::wcerr << L"[Error] Failed to allocate memory in game process (Code: " << err << L")\n";
        if (err == ERROR_ACCESS_DENIED) {
            std::wcerr << L"Hint: Try running CrimsonSwitchLauncher as Administrator (Right-click -> Run as administrator).\n";
        } else if (err == ERROR_INVALID_HANDLE) {
            std::wcerr << L"Hint: Game process handle is invalid or the process already exited.\n";
        }
        return false;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMem, dllPath.c_str(), pathSize, &bytesWritten) || bytesWritten != pathSize) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    LPVOID pLoadLibraryW = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!pLoadLibraryW) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW), pRemoteMem, 0, nullptr);
    if (!hThread) {
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    DWORD waitRes = WaitForSingleObject(hThread, 8000);
    if (waitRes != WAIT_OBJECT_0) {
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);

    if (exitCode == 0) {
        std::wcerr << L"[Error] LoadLibraryW returned NULL inside the game process!\n";
        return false;
    }

    std::wcout << L"  -> [SUCCESS] Injected at remote module handle: 0x" << std::hex << exitCode << std::dec << L"\n";
    return true;
}

} // namespace SwitchLauncher

int main() {
    SwitchLauncher::EnableDebugPrivilege();

    std::cout << "=========================================================\n";
    std::cout << "   Crimson Desert Early Trio Switch Standalone Launcher \n";
    std::cout << "   Instant Switching: Kliff, Damiane, Oongka             \n";
    std::cout << "=========================================================\n\n";

    std::wstring modDllPath = SwitchLauncher::FindModDll();
    if (modDllPath.empty()) {
        std::wcerr << L"[Error] Could not find CrimsonSwitch.dll!\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }
    std::wcout << L"[Launcher] Found Mod DLL: " << modDllPath << L"\n";

    // 1. Check if game is ALREADY running (Hot-attach mode)
    DWORD existingPid = SwitchLauncher::FindProcessId(SwitchLauncher::GAME_EXE_NAME);
    if (existingPid != 0) {
        std::wcout << L"[Launcher] Detected running Crimson Desert (PID: " << existingPid << L")!\n";
        std::cout << "[Launcher] Attaching CrimsonSwitch into running game...\n";

        HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, existingPid);
        if (hProcess) {
            if (SwitchLauncher::InjectDll(hProcess, modDllPath)) {
                std::cout << "\n=========================================================\n";
                std::cout << "  Early Trio Unlocked in Running Game!                   \n";
                std::cout << "  -> Radial wheel unlocked for Damiane and Oongka.       \n";
                std::cout << "  -> Direct hotkeys: [1] Kliff, [2] Damiane, [3] Oongka  \n";
                std::cout << "=========================================================\n";
            }
            CloseHandle(hProcess);
        }
        Sleep(3000);
        return 0;
    }

    // 2. Launch game from scratch
    std::wstring gameExePath = SwitchLauncher::FindGameExecutable();
    if (gameExePath.empty()) {
        std::cout << "Please enter the full path to CrimsonDesert.exe:\n> ";
        std::string userPath;
        std::getline(std::cin, userPath);
        std::wstring rawPath(userPath.begin(), userPath.end());
        gameExePath = SwitchLauncher::CleanPath(rawPath);
    }

    if (!fs::exists(gameExePath)) {
        std::wcerr << L"[Error] Game executable not found at: " << gameExePath << L"\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }

    std::wcout << L"[Launcher] Target: " << gameExePath << L"\n";
    std::cout << "[Launcher] Launching Crimson Desert with Early Trio Unlock...\n";

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    fs::path gameDir = fs::path(gameExePath).parent_path();

    BOOL success = CreateProcessW(
        gameExePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        gameDir.c_str(),
        &si,
        &pi
    );

    if (!success) {
        std::wcerr << L"[Error] Failed to launch game process (Code: " << GetLastError() << L")\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }

    std::cout << "[Launcher] Game process started (PID: " << pi.dwProcessId << "). Initializing...\n";
    Sleep(2500);

    HANDLE hTargetProcess = pi.hProcess;
    bool reopened = false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
        std::cout << "[Launcher] Steam client handoff detected (PID " << pi.dwProcessId << " exited with code " << exitCode << ").\n";
        std::cout << "[Launcher] Waiting for main game process to spawn under Steam...\n";

        hTargetProcess = nullptr;
        for (int i = 0; i < 30; ++i) {
            Sleep(500);
            DWORD activePid = SwitchLauncher::FindProcessId(SwitchLauncher::GAME_EXE_NAME);
            if (activePid != 0 && activePid != pi.dwProcessId) {
                hTargetProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, activePid);
                if (hTargetProcess) {
                    std::wcout << L"[Launcher] Attached to active Crimson Desert process (PID: " << activePid << L")!\n";
                    reopened = true;
                    Sleep(2000);
                    break;
                }
            }
        }

        if (!hTargetProcess) {
            std::wcerr << L"[Error] Could not locate active game process after Steam handoff.\n";
            std::cout << "Tip: Launch Crimson Desert through Steam, then double-click CrimsonSwitchLauncher.\n";
            std::cout << "\nPress ENTER to exit...";
            std::cin.get();
            return 1;
        }
    }

    if (SwitchLauncher::InjectDll(hTargetProcess, modDllPath)) {
        std::cout << "  -> [SUCCESS] Early Trio switch mod active!\n";
        std::cout << "  -> Direct hotkeys: [Alt+1] Kliff, [Alt+2] Damiane, [Alt+3] Oongka, [Alt+C] Cycle\n";
    }

    if (reopened && hTargetProcess) {
        CloseHandle(hTargetProcess);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    Sleep(3000);
    return 0;
}
