#pragma once
#include <Windows.h>
#include <string>

bool SetupHook(const std::wstring& dllPath, const std::wstring& functionName, DWORD threadId, DWORD processId = 0, HWND hwnd = NULL);
bool RemoveHook();