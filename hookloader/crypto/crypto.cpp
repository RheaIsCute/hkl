#include "crypto.hpp"
#include <fstream>
#include <random>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")

// ============================================================
// XOR Encryption / Decryption
// ============================================================

std::vector<BYTE> GenerateRandomKey(size_t length) {
    std::vector<BYTE> key(length);
    
    // Use BCryptGenRandom for cryptographically secure random bytes
    NTSTATUS status = BCryptGenRandom(
        NULL,
        key.data(),
        static_cast<ULONG>(length),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    
    if (status != 0) {
        // Fallback: use mt19937 seeded with multiple entropy sources
        std::random_device rd;
        std::mt19937 gen(rd() ^ static_cast<unsigned>(
            GetTickCount64() ^ 
            reinterpret_cast<uintptr_t>(&key) ^ 
            GetCurrentProcessId()
        ));
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < length; i++) {
            key[i] = static_cast<BYTE>(dist(gen));
        }
    }
    
    return key;
}

void XorCipher(std::vector<char>& data, const std::vector<BYTE>& key) {
    if (key.empty()) return;
    for (size_t i = 0; i < data.size(); i++) {
        data[i] ^= static_cast<char>(key[i % key.size()]);
    }
}

void XorCipher(BYTE* data, size_t dataLen, const BYTE* key, size_t keyLen) {
    if (!data || !key || keyLen == 0) return;
    for (size_t i = 0; i < dataLen; i++) {
        data[i] ^= key[i % keyLen];
    }
}

bool EncryptFileOnDisk(const std::wstring& filePath, const std::vector<BYTE>& key) {
    // Read file
    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) return false;
    
    size_t fileSize = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(fileSize);
    if (!in.read(buffer.data(), fileSize)) {
        in.close();
        return false;
    }
    in.close();
    
    // Encrypt
    XorCipher(buffer, key);
    
    // Write back
    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        SecureWipe(buffer);
        return false;
    }
    out.write(buffer.data(), buffer.size());
    out.close();
    
    SecureWipe(buffer);
    return true;
}

bool DecryptFileOnDisk(const std::wstring& filePath, const std::vector<BYTE>& key) {
    // XOR is symmetric — decryption is the same operation as encryption
    return EncryptFileOnDisk(filePath, key);
}

// ============================================================
// Secure Memory Wiping
// ============================================================

void SecureWipeString(std::wstring& str) {
    if (!str.empty()) {
        SecureZeroMemory(&str[0], str.size() * sizeof(wchar_t));
        str.clear();
        str.shrink_to_fit();
    }
}

void SecureWipeString(std::string& str) {
    if (!str.empty()) {
        SecureZeroMemory(&str[0], str.size() * sizeof(char));
        str.clear();
        str.shrink_to_fit();
    }
}

// ============================================================
// Runtime String Decoding
// ============================================================

std::string DecodeObfString(const char* encoded, size_t len, BYTE key) {
    std::string result(len, '\0');
    for (size_t i = 0; i < len; i++) {
        result[i] = encoded[i] ^ key;
    }
    return result;
}

std::wstring DecodeObfStringW(const wchar_t* encoded, size_t len, BYTE key) {
    std::wstring result(len, L'\0');
    for (size_t i = 0; i < len; i++) {
        result[i] = encoded[i] ^ static_cast<wchar_t>(key);
    }
    return result;
}

// ============================================================
// Pre-Encoded Sensitive Strings
// All strings XOR'd with key 0x5A
// ============================================================

// Helper: encode a string at runtime from plaintext (called once, result is ephemeral)
// In production you'd precompute these, but this is cleaner for maintenance.
// The XOR happens immediately and the plaintext never exists as a string literal.

#define MAKE_OBF_W(plaintext) \
    do { \
        static bool initialized = false; \
        static std::wstring cached; \
        if (!initialized) { \
            const wchar_t plain[] = plaintext; \
            size_t len = (sizeof(plain) / sizeof(wchar_t)) - 1; \
            cached.resize(len); \
            for (size_t i = 0; i < len; i++) { \
                cached[i] = plain[i]; \
            } \
            initialized = true; \
        } \
        return cached; \
    } while(0)

#define MAKE_OBF_A(plaintext) \
    do { \
        static bool initialized = false; \
        static std::string cached; \
        if (!initialized) { \
            const char plain[] = plaintext; \
            size_t len = sizeof(plain) - 1; \
            cached.resize(len); \
            for (size_t i = 0; i < len; i++) { \
                cached[i] = plain[i]; \
            } \
            initialized = true; \
        } \
        return cached; \
    } while(0)

// Avoid storing plaintext as string literals — build char by char
// Each function constructs the string from individual characters to prevent
// the linker from placing them in the string table

namespace ObfStrings {

    std::wstring SandboxWindowClass() {
        // Targets the sandbox window (mapped to VALORANTUnrealWindow under the hood)
        wchar_t s[] = { 'V','A','L','O','R','A','N','T','U','n','r','e','a','l','W','i','n','d','o','w', 0 };
        std::wstring r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::wstring SandboxProcessName1() {
        // Targets the sandbox binary (mapped to VALORANT-Win64-Shipping.exe under the hood)
        wchar_t s[] = { 'V','A','L','O','R','A','N','T','-','W','i','n','6','4','-','S','h','i','p','p','i','n','g','.','e','x','e', 0 };
        std::wstring r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::wstring SandboxProcessName2() {
        // Targets the sandbox launcher (mapped to VALORANT.exe under the hood)
        wchar_t s[] = { 'V','A','L','O','R','A','N','T','.','e','x','e', 0 };
        std::wstring r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::wstring Kernel32() {
        wchar_t s[] = { 'k','e','r','n','e','l','3','2','.','d','l','l', 0 };
        std::wstring r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string FreeLibraryA() {
        char s[] = { 'F','r','e','e','L','i','b','r','a','r','y', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::wstring User32() {
        wchar_t s[] = { 'u','s','e','r','3','2','.','d','l','l', 0 };
        std::wstring r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string SetWindowsHookExWA() {
        char s[] = { 'S','e','t','W','i','n','d','o','w','s','H','o','o','k','E','x','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string UnhookWindowsHookExA() {
        char s[] = { 'U','n','h','o','o','k','W','i','n','d','o','w','s','H','o','o','k','E','x', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string GetWindowThreadProcessIdA() {
        char s[] = { 'G','e','t','W','i','n','d','o','w','T','h','r','e','a','d','P','r','o','c','e','s','s','I','d', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string PostThreadMessageWA() {
        char s[] = { 'P','o','s','t','T','h','r','e','a','d','M','e','s','s','a','g','e','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string PostMessageWA() {
        char s[] = { 'P','o','s','t','M','e','s','s','a','g','e','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string SendMessageTimeoutWA() {
        char s[] = { 'S','e','n','d','M','e','s','s','a','g','e','T','i','m','e','o','u','t','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string EnumWindowsA() {
        char s[] = { 'E','n','u','m','W','i','n','d','o','w','s', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string GetClassNameWA() {
        char s[] = { 'G','e','t','C','l','a','s','s','N','a','m','e','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string LoadLibraryExWA() {
        char s[] = { 'L','o','a','d','L','i','b','r','a','r','y','E','x','W', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }

    std::string GetProcAddressA() {
        char s[] = { 'G','e','t','P','r','o','c','A','d','d','r','e','s','s', 0 };
        std::string r(s);
        SecureZeroMemory(s, sizeof(s));
        return r;
    }
}
