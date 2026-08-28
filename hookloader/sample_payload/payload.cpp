#include <windows.h>
#include <tlhelp32.h>
#include <string>

HHOOK legitMouseHook = NULL;
HHOOK legitKeyboardHook = NULL;
HMODULE gModule = NULL;

// Exported hook callback required for Hookloader (SetWindowsHookEx) injection
extern "C" __declspec(dllexport) LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

DWORD GetDiscordPID() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    DWORD pid = 0;

    if (Process32First(snapshot, &pe32)) {
        do {
            if (std::string(pe32.szExeFile) == "Discord.exe") {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }
    CloseHandle(snapshot);
    return pid;
}

void RestartDiscord() {
    DWORD pid = GetDiscordPID();
    if (pid) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
    }

    Sleep(2000);

    const char* localAppData = getenv("LOCALAPPDATA");
    if (!localAppData) return;

    std::string appdata = std::string(localAppData);
    std::string path = appdata + "\\Discord\\Update.exe";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    std::string cmd = path + " --processStart Discord.exe";
    
    if (CreateProcessA(
        NULL,
        const_cast<LPSTR>(cmd.c_str()),
        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi
    )) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    Sleep(5000);
}

HWND WaitForDiscordWindow() {
    HWND hwnd = NULL;
    int attempts = 0;
    while (!hwnd && attempts < 30) {
        hwnd = FindWindowA("Discord", NULL);
        if (!hwnd) {
            Sleep(1000);
            attempts++;
        }
    }
    return hwnd;
}

void RenderTotallyLegitOverlay(HWND discordHwnd) {
    RECT rect;
    if (GetWindowRect(discordHwnd, &rect)) {
        // Overlay rendering logic
    }
}

void DebugLog(const char* message) {
    OutputDebugStringA(message);
}

LRESULT CALLBACK MouseHookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_MOUSEMOVE) {
            DebugLog("Checking mouse interaction...");
        }
    }
    return CallNextHookEx(legitMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            DebugLog("Checking keyboard accessibility...");
        }
    }
    return CallNextHookEx(legitKeyboardHook, nCode, wParam, lParam);
}

DWORD WINAPI TotallyLegitMain(LPVOID lpParam) {
    HMODULE hModule = (HMODULE)lpParam;
    gModule = hModule;

    RestartDiscord();

    HWND discordHwnd = WaitForDiscordWindow();
    if (!discordHwnd) {
        FreeLibraryAndExitThread(hModule, 0);
        return 0;
    }

    legitMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookCallback, hModule, 0);
    legitKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hModule, 0);

    while (true) {
        if (!IsWindow(discordHwnd)) {
            RestartDiscord();
            discordHwnd = WaitForDiscordWindow();
            if (!discordHwnd) break;

            if (legitMouseHook) UnhookWindowsHookEx(legitMouseHook);
            if (legitKeyboardHook) UnhookWindowsHookEx(legitKeyboardHook);

            legitMouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookCallback, hModule, 0);
            legitKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hModule, 0);
        }

        RenderTotallyLegitOverlay(discordHwnd);

        if (GetAsyncKeyState(VK_DELETE)) break;

        Sleep(10);
    }

    if (legitMouseHook) {
        UnhookWindowsHookEx(legitMouseHook);
        legitMouseHook = NULL;
    }
    if (legitKeyboardHook) {
        UnhookWindowsHookEx(legitKeyboardHook);
        legitKeyboardHook = NULL;
    }
    
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TotallyLegitMain, (LPVOID)hModule, 0, NULL);
            break;
        case DLL_PROCESS_DETACH:
            if (legitMouseHook) {
                UnhookWindowsHookEx(legitMouseHook);
                legitMouseHook = NULL;
            }
            if (legitKeyboardHook) {
                UnhookWindowsHookEx(legitKeyboardHook);
                legitKeyboardHook = NULL;
            }
            break;
    }
    return TRUE;
}
