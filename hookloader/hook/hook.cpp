#include "hook.hpp"
#include "../crypto/crypto.hpp"
#include "../stealth/stealth.hpp"
#include "../discord_overlay/discord_overlay.hpp"
#include <iostream>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>

#pragma comment(lib, "psapi.lib")

static HHOOK g_hookHandle = nullptr;
static HMODULE g_dllModule = nullptr;
static HWND g_targetHwnd = nullptr;
static DWORD g_targetTid = 0;
static DWORD g_targetPid = 0;
static std::wstring g_dllPath;
static bool g_usedOverlayMasking = false;
static std::wstring g_overlayStaged;

static std::wstring GetFileName(const std::wstring& path) {
    size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        return path.substr(lastSlash + 1);
    }
    return path;
}

static HMODULE FindRemoteModule(HANDLE hProcess, DWORD pid, const std::wstring& dllPath) {
    std::wstring targetFileName = GetFileName(dllPath);

    // Method 1: EnumProcessModulesEx (PSAPI) — still using static import here since
    // PSAPI functions are less monitored than kernel32/user32 injection APIs
    HMODULE hMods[1024];
    DWORD cbNeeded = 0;
    if (EnumProcessModulesEx(hProcess, hMods, sizeof(hMods), &cbNeeded, LIST_MODULES_ALL)) {
        DWORD count = cbNeeded / sizeof(HMODULE);
        for (DWORD i = 0; i < count; i++) {
            wchar_t szModPath[MAX_PATH] = { 0 };
            if (GetModuleFileNameExW(hProcess, hMods[i], szModPath, MAX_PATH)) {
                std::wstring modPath(szModPath);
                std::wstring modFileName = GetFileName(modPath);

                if (_wcsicmp(modFileName.c_str(), targetFileName.c_str()) == 0 ||
                    _wcsicmp(modPath.c_str(), dllPath.c_str()) == 0) {
                    SecureZeroMemory(szModPath, sizeof(szModPath));
                    return hMods[i];
                }
                SecureZeroMemory(szModPath, sizeof(szModPath));
            }
        }
    }

    // Method 2: Toolhelp32 Snapshot fallback
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = { sizeof(me) };
        if (Module32FirstW(hSnap, &me)) {
            do {
                if (_wcsicmp(me.szModule, targetFileName.c_str()) == 0 ||
                    _wcsicmp(me.szExePath, dllPath.c_str()) == 0) {
                    CloseHandle(hSnap);
                    return (HMODULE)me.modBaseAddr;
                }
            } while (Module32NextW(hSnap, &me));
        }
        CloseHandle(hSnap);
    }

    return nullptr;
}

static bool ForceUnloadRemoteModule(DWORD pid, const std::wstring& dllPath) {
    if (pid == 0 || dllPath.empty()) {
        std::cout << "[-] Invalid parameters for remote unload." << std::endl;
        return false;
    }

    // Use stealth-resolved OpenProcess
    HANDLE hProcess = nullptr;
    if (StealthAPI::pOpenProcess) {
        hProcess = StealthAPI::pOpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            hProcess = StealthAPI::pOpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                FALSE, pid);
        }
    } else {
        hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            hProcess = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                FALSE, pid);
        }
    }

    if (!hProcess) {
        std::cout << "[-] Failed to open target process (Error: " << GetLastError() << ")" << std::endl;
        return false;
    }

    // Resolve FreeLibrary from kernel32 using stealth
    std::wstring k32name = ObfStrings::Kernel32();
    HMODULE hKernel32 = GetModuleHandleW(k32name.c_str());
    SecureWipeString(k32name);

    std::string flName = ObfStrings::FreeLibraryA();
    FARPROC pFreeLib = nullptr;
    if (StealthAPI::pGetProcAddress && hKernel32) {
        pFreeLib = StealthAPI::pGetProcAddress(hKernel32, flName.c_str());
    } else if (hKernel32) {
        pFreeLib = ::GetProcAddress(hKernel32, flName.c_str());
    }
    SecureWipeString(flName);

    if (!pFreeLib) {
        std::cout << "[-] Could not resolve unload function" << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    bool successfullyUnloaded = false;

    // Loop up to 10 times to clear all reference counts in target process
    for (int attempt = 1; attempt <= 10; ++attempt) {
        HMODULE hRemoteMod = FindRemoteModule(hProcess, pid, dllPath);
        if (!hRemoteMod) {
            if (attempt > 1) {
                std::cout << "[+] Module completely unmapped after " << (attempt - 1) << " pass(es)." << std::endl;
                successfullyUnloaded = true;
            } else {
                std::cout << "[*] Module was not found (already unloaded)." << std::endl;
                successfullyUnloaded = true;
            }
            break;
        }

        std::cout << "[+] Found remote module at: 0x" << std::hex << (uintptr_t)hRemoteMod << std::dec << " (Attempt " << attempt << ")" << std::endl;

        // Use stealth-resolved CreateRemoteThread
        HANDLE hThread = nullptr;
        if (StealthAPI::pCreateRemoteThread) {
            hThread = StealthAPI::pCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFreeLib, (LPVOID)hRemoteMod, 0, NULL);
        } else {
            hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFreeLib, (LPVOID)hRemoteMod, 0, NULL);
        }

        if (hThread) {
            WaitForSingleObject(hThread, 2000);
            DWORD exitCode = 0;
            GetExitCodeThread(hThread, &exitCode);
            CloseHandle(hThread);
            std::cout << "[+] Remote unload executed (Exit Code: " << exitCode << ")" << std::endl;

            // Jittered delay between attempts
            JitteredSleep(50, 150);
        } else {
            std::cout << "[-] Remote thread creation failed (Error: " << GetLastError() << ")" << std::endl;
            break;
        }
    }

    CloseHandle(hProcess);
    return successfullyUnloaded;
}

bool SetupHook(const std::wstring& dllPath, const std::wstring& functionName, DWORD threadId, DWORD processId, HWND hwnd) {
    g_dllPath = dllPath;
    g_targetTid = threadId;
    g_targetPid = processId;
    g_targetHwnd = hwnd;
    g_usedOverlayMasking = false;
    g_overlayStaged.clear();

    // Jittered delay before loading to break timing patterns
    JitteredSleep(30, 120);

    // ---- DISCORD OVERLAY MASKING ----
    // Try to stage the DLL in Discord's overlay directory first.
    // This makes the LoadLibrary call look like a legitimate Discord overlay
    // component being loaded, which is whitelisted by most anti-cheats.
    
    std::wstring loadPath = dllPath; // Default: load from original path
    
    if (IsDiscordInstalled()) {
        std::cout << "[+] Discord detected — attempting overlay masking..." << std::endl;
        
        std::wstring stagedPath = StagePayloadAsOverlay(dllPath);
        if (!stagedPath.empty()) {
            loadPath = stagedPath;
            g_overlayStaged = stagedPath;
            g_usedOverlayMasking = true;
            SetStagedOverlayPath(stagedPath);
            std::cout << "[+] Payload staged in overlay directory" << std::endl;
        } else {
            std::cout << "[!] Overlay staging failed, falling back to direct method" << std::endl;
        }
    } else {
        std::cout << "[*] Discord not detected — using direct injection method" << std::endl;
    }

    std::cout << "[+] Loading module..." << std::endl;
    
    // Use stealth-resolved LoadLibraryExW
    if (StealthAPI::pLoadLibraryExW) {
        g_dllModule = StealthAPI::pLoadLibraryExW(loadPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    } else {
        g_dllModule = LoadLibraryExW(loadPath.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    }
    
    if (!g_dllModule) {
        std::cout << "[-] Module load failed" << std::endl;
        // Clean up staged overlay file on failure
        if (g_usedOverlayMasking) {
            CleanupOverlayPayload(g_overlayStaged);
            g_usedOverlayMasking = false;
            g_overlayStaged.clear();
        }
        return false;
    }

    JitteredSleep(20, 80);

    std::cout << "[+] Resolving entry point..." << std::endl;
    
    // Use stealth-resolved GetProcAddress
    std::string funcNameA(functionName.begin(), functionName.end());
    HOOKPROC addr = nullptr;
    if (StealthAPI::pGetProcAddress) {
        addr = (HOOKPROC)StealthAPI::pGetProcAddress(g_dllModule, funcNameA.c_str());
    } else {
        addr = (HOOKPROC)::GetProcAddress(g_dllModule, funcNameA.c_str());
    }
    SecureWipeString(funcNameA);

    if (!addr) {
        std::cout << "[-] Entry point not found" << std::endl;
        if (StealthAPI::pFreeLibrary) {
            StealthAPI::pFreeLibrary(g_dllModule);
        } else {
            FreeLibrary(g_dllModule);
        }
        g_dllModule = nullptr;
        if (g_usedOverlayMasking) {
            CleanupOverlayPayload(g_overlayStaged);
            g_usedOverlayMasking = false;
            g_overlayStaged.clear();
        }
        return false;
    }

    JitteredSleep(30, 100);

    std::cout << "[+] Installing handler..." << std::endl;
    
    // Use stealth-resolved SetWindowsHookExW
    if (StealthAPI::pSetWindowsHookExW) {
        g_hookHandle = StealthAPI::pSetWindowsHookExW(WH_GETMESSAGE, addr, g_dllModule, threadId);
    } else {
        g_hookHandle = SetWindowsHookExW(WH_GETMESSAGE, addr, g_dllModule, threadId);
    }
    
    if (!g_hookHandle) {
        std::cout << "[-] Handler installation failed" << std::endl;
        if (StealthAPI::pFreeLibrary) {
            StealthAPI::pFreeLibrary(g_dllModule);
        } else {
            FreeLibrary(g_dllModule);
        }
        g_dllModule = nullptr;
        if (g_usedOverlayMasking) {
            CleanupOverlayPayload(g_overlayStaged);
            g_usedOverlayMasking = false;
            g_overlayStaged.clear();
        }
        return false;
    }

    if (g_usedOverlayMasking) {
        std::cout << "[+] Hook installed via Discord overlay masking" << std::endl;
    }

    // Trigger the target thread to process the hook using stealth-resolved PostMessage
    if (StealthAPI::pPostThreadMessageW) {
        StealthAPI::pPostThreadMessageW(threadId, WM_NULL, NULL, NULL);
    } else {
        PostThreadMessageW(threadId, WM_NULL, NULL, NULL);
    }

    if (hwnd) {
        bool isWnd = StealthAPI::pIsWindow ? StealthAPI::pIsWindow(hwnd) : IsWindow(hwnd);
        if (isWnd) {
            if (StealthAPI::pPostMessageW) {
                StealthAPI::pPostMessageW(hwnd, WM_NULL, 0, 0);
            } else {
                PostMessageW(hwnd, WM_NULL, 0, 0);
            }
        }
    }

    return true;
}

bool RemoveHook() {
    bool success = true;

    std::cout << "[+] Safe detach sequence initiated..." << std::endl;

    if (g_hookHandle) {
        BOOL result;
        if (StealthAPI::pUnhookWindowsHookEx) {
            result = StealthAPI::pUnhookWindowsHookEx(g_hookHandle);
        } else {
            result = UnhookWindowsHookEx(g_hookHandle);
        }

        if (!result) {
            std::cout << "[-] Detach returned false (Error: " << GetLastError() << ")" << std::endl;
            success = false;
        } else {
            std::cout << "[+] Handler detached successfully." << std::endl;
        }
        g_hookHandle = nullptr;
    }

    // Force target window and thread to process messages so OS releases hook references
    if (g_targetHwnd) {
        bool isWnd = StealthAPI::pIsWindow ? StealthAPI::pIsWindow(g_targetHwnd) : IsWindow(g_targetHwnd);
        if (isWnd) {
            if (StealthAPI::pPostMessageW) {
                StealthAPI::pPostMessageW(g_targetHwnd, WM_NULL, 0, 0);
            } else {
                PostMessageW(g_targetHwnd, WM_NULL, 0, 0);
            }
            if (StealthAPI::pSendMessageTimeoutW) {
                StealthAPI::pSendMessageTimeoutW(g_targetHwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 150, NULL);
            } else {
                SendMessageTimeoutW(g_targetHwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 150, NULL);
            }
        }
    }
    if (g_targetTid) {
        if (StealthAPI::pPostThreadMessageW) {
            StealthAPI::pPostThreadMessageW(g_targetTid, WM_NULL, 0, 0);
        } else {
            PostThreadMessageW(g_targetTid, WM_NULL, 0, 0);
        }
    }
    
    JitteredSleep(150, 300);

    // Eject DLL module from remote process memory
    // Use the overlay-staged path if overlay masking was used
    std::wstring ejectPath = g_usedOverlayMasking && !g_overlayStaged.empty() ? g_overlayStaged : g_dllPath;
    if (g_targetPid != 0 && !ejectPath.empty()) {
        std::cout << "[+] Ejecting module from target..." << std::endl;
        if (!ForceUnloadRemoteModule(g_targetPid, ejectPath)) {
            success = false;
        }
    }

    if (g_dllModule) {
        if (StealthAPI::pFreeLibrary) {
            StealthAPI::pFreeLibrary(g_dllModule);
        } else {
            FreeLibrary(g_dllModule);
        }
        g_dllModule = nullptr;
    }

    // Securely delete the temp DLL file from disk
    if (!g_dllPath.empty()) {
        std::cout << "[+] Securely erasing temp module from disk..." << std::endl;
        SecureDeleteFile(g_dllPath);
    }

    // Clean up overlay-staged payload from Discord directory
    if (g_usedOverlayMasking && !g_overlayStaged.empty()) {
        std::cout << "[+] Cleaning overlay-staged payload..." << std::endl;
        CleanupOverlayPayload(g_overlayStaged);
        SecureWipeString(g_overlayStaged);
        g_usedOverlayMasking = false;
    }

    g_targetTid = 0;
    g_targetPid = 0;
    g_targetHwnd = nullptr;

    // Scrub the DLL path from memory
    SecureWipeString(g_dllPath);

    return success;
}