#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <array>

// ============================================================
// XOR Encryption / Decryption
// ============================================================

// Generate a cryptographically secure random key of the given length
std::vector<BYTE> GenerateRandomKey(size_t length);

// XOR encrypt/decrypt a buffer in-place using the given key
void XorCipher(std::vector<char>& data, const std::vector<BYTE>& key);
void XorCipher(BYTE* data, size_t dataLen, const BYTE* key, size_t keyLen);

// Encrypt a file on disk in-place using the given key
bool EncryptFileOnDisk(const std::wstring& filePath, const std::vector<BYTE>& key);

// Decrypt a file on disk in-place using the given key
bool DecryptFileOnDisk(const std::wstring& filePath, const std::vector<BYTE>& key);

// ============================================================
// Secure Memory Wiping
// ============================================================

// Wipe a vector's contents securely
template<typename T>
void SecureWipe(std::vector<T>& vec) {
    if (!vec.empty()) {
        SecureZeroMemory(vec.data(), vec.size() * sizeof(T));
        vec.clear();
        vec.shrink_to_fit();
    }
}

// Wipe a wstring's contents securely
void SecureWipeString(std::wstring& str);
void SecureWipeString(std::string& str);

// ============================================================
// Compile-Time String Obfuscation
// ============================================================
// Usage: auto s = ObfuscatedString<XorKey, 'H','e','l','l','o'>();
//        std::wstring decoded = s.decode();

template<BYTE Key, char... Chars>
struct ObfuscatedStringA {
    static constexpr size_t N = sizeof...(Chars);
    // Encrypted at compile time
    char data[N + 1] = { static_cast<char>(Chars ^ Key)..., '\0' };

    // Decrypt at runtime
    std::string decode() const {
        std::string result(N, '\0');
        for (size_t i = 0; i < N; i++) {
            result[i] = data[i] ^ Key;
        }
        return result;
    }
};

template<BYTE Key, wchar_t... Chars>
struct ObfuscatedStringW {
    static constexpr size_t N = sizeof...(Chars);
    wchar_t data[N + 1] = { static_cast<wchar_t>(Chars ^ Key)..., L'\0' };

    std::wstring decode() const {
        std::wstring result(N, L'\0');
        for (size_t i = 0; i < N; i++) {
            result[i] = data[i] ^ Key;
        }
        return result;
    }
};

// Helper macros for easier usage
// XOR key 0x5A chosen as non-trivial single-byte key
#define OBF_KEY 0x5A

// Runtime string decode helpers (for strings that can't use templates)
std::string DecodeObfString(const char* encoded, size_t len, BYTE key);
std::wstring DecodeObfStringW(const wchar_t* encoded, size_t len, BYTE key);

// Pre-encoded sensitive strings (encoded with OBF_KEY = 0x5A at compile time)
// These are decoded at runtime only when needed

namespace ObfStrings {
    // "SandboxUnrealWindow" (Targets sandbox environment window)
    std::wstring SandboxWindowClass();
    // "Sandbox-Win64-Shipping.exe" (Targets sandbox environment process)
    std::wstring SandboxProcessName1();
    // "Sandbox.exe" (Targets sandbox environment process)
    std::wstring SandboxProcessName2();
    // "kernel32.dll"
    std::wstring Kernel32();
    // "FreeLibrary"
    std::string FreeLibraryA();
    // "user32.dll"
    std::wstring User32();
    // "SetWindowsHookExW"
    std::string SetWindowsHookExWA();
    // "UnhookWindowsHookEx"
    std::string UnhookWindowsHookExA();
    // "GetWindowThreadProcessId"
    std::string GetWindowThreadProcessIdA();
    // "PostThreadMessageW"
    std::string PostThreadMessageWA();
    // "PostMessageW"
    std::string PostMessageWA();
    // "SendMessageTimeoutW"
    std::string SendMessageTimeoutWA();
    // "EnumWindows"
    std::string EnumWindowsA();
    // "GetClassNameW"
    std::string GetClassNameWA();
    // "LoadLibraryExW"
    std::string LoadLibraryExWA();
    // "GetProcAddress"
    std::string GetProcAddressA();
}
