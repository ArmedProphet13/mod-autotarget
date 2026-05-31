#include "Bootstrap/Initializer.h"
#include "Bootstrap/Version.h"

#include <cctype>
#include <cstdint>
#include <string>

#include "MinHook.h"

#include "Config/ConfigManager.h"
#include "Diagnostics/Logger.h"
#include "Diagnostics/SafeMode.h"
#include "GameInterface/MouseoverProbe.h"
#include "GameInterface/Hooks/FrameHook.h"
#include "GameInterface/Hooks/SelectionHook.h"
#include "GameInterface/Hooks/SpellCastHook.h"
#include "GameInterface/Hooks/TabHook.h"
#include "Input/EnableToggle.h"
#include "Orchestration/AutoTargetController.h"

namespace autotarget {
namespace Initializer {

namespace {

// WoW.exe build 12340 always images here. Every offset in Offsets.h is anchored
// to this base, so a mismatch means we are not in the supported client.
constexpr std::uintptr_t kExpectedImageBase = 0x00400000;

constexpr std::uint32_t kUiInstallDelayMs   = 5000;  // let FrameXML finish loading
constexpr int           kFrameHookAttempts  = 60;    // x 500ms = 30s for the device
constexpr DWORD         kFrameHookRetryMs    = 500;

HMODULE              g_self = nullptr;
ConfigManager        g_config;
AutoTargetController* g_controller = nullptr;

bool          g_uiAttempted = false; // set once we have tried to install the UI
bool          g_uiInstalled = false;  // true only when the Lua UI is actually present
bool          g_live = false;         // true only in RunMode::Live
bool          g_probeArmed = false;
std::uint32_t g_inWorldSinceMs = 0;

// Directory this DLL lives in (== the WoW directory, since it ships beside
// WoW.exe). Returned with a trailing backslash.
std::string ModuleDirectory() {
    char path[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameA(g_self, path, MAX_PATH);
    std::string s(path, n);
    const size_t slash = s.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : s.substr(0, slash + 1);
}

// Confirms we are inside the supported 12340 client before any offset is used.
bool VerifyClient() {
    const HMODULE exe = GetModuleHandleA(nullptr);
    if (reinterpret_cast<std::uintptr_t>(exe) != kExpectedImageBase) {
        AT_LOG_ERROR("VerifyClient: unexpected image base %p (want %p)",
                     exe, reinterpret_cast<void*>(kExpectedImageBase));
        return false;
    }

    // The exe filename used to be required to be "wow.exe", but private servers
    // routinely rename the launcher executable (Ascension.exe, Wow-64.exe etc.
    // shipped alongside a renamed Wow.exe). The image-base check above is the
    // real safety net - anything other than 0x00400000 cannot be 12340 - so
    // the filename is now just logged for diagnostics.
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string name(exePath);
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    AT_LOG_INFO("VerifyClient: host exe = '%s' (image base OK)", name.c_str());
    return true;
}

// The per-frame body, run under SafeMode. Plain function, no captures.
void FrameStep(void* /*ctx*/) {
    if (g_controller == nullptr)
        return;

    // Mouseover-slot probe is off - the client writers were already located
    // at 0x0051F7D1 / F7F1 / F832 and the reader at 0x0060AEDC. Re-enable
    // here if those need re-verifying on a different client.
    (void)g_probeArmed;

    // The in-game checkbox / slash command run Lua in the client. Only Live
    // mode installs them, and only once the player is in the world: the
    // login/character-select screen runs a different Lua environment (GlueXML)
    // that has neither SlashCmdList nor the Interface Options panels.
    if (g_live && !g_uiAttempted && g_controller->InWorld()) {
        const std::uint32_t now = GetTickCount();
        if (g_inWorldSinceMs == 0)
            g_inWorldSinceMs = now;
        if (now - g_inWorldSinceMs > kUiInstallDelayMs) {
            // g_uiInstalled tracks whether the Lua UI is present; g_uiAttempted
            // ensures we only try once per session either way (a failed install
            // does not retry).
            g_uiInstalled = EnableToggle::Install(g_controller);
            g_uiAttempted = true;
        }
    }

    g_controller->OnFrame();
}

// Handed to FrameHook; runs on the game thread once per rendered frame.
void OnFrame() {
    SafeMode::Run(&FrameStep, nullptr, "frame");
}

DWORD WINAPI InitThread(LPVOID) {
    const std::string dir = ModuleDirectory();

    Logger::Init(dir + "AutoTarget.log", LogLevel::Info);
    AT_LOG_INFO("AutoTarget v%s starting up (pid=%lu, module=%p)",
                AUTOTARGET_VERSION_STRING, GetCurrentProcessId(), g_self);

    if (!VerifyClient()) {
        AT_LOG_ERROR("AutoTarget self-disabled: unsupported client");
        return 0;
    }

    g_config.Load(dir + "AutoTarget.ini");
    Logger::SetLevel(g_config.Get().logLevel);

    const RunMode mode = g_config.Get().mode;
    g_live = (mode == RunMode::Live);

    if (mode == RunMode::Off) {
        AT_LOG_WARN("Mode=off: AutoTarget is loaded and forwarding graphics, "
                    "but installs no hook. Set Mode=diagnostic to begin.");
        return 0;
    }

    if (MH_Initialize() != MH_OK) {
        AT_LOG_ERROR("AutoTarget self-disabled: MH_Initialize failed");
        return 0;
    }

    g_controller = new AutoTargetController(g_config);

    if (mode == RunMode::Diagnostic) {
        AT_LOG_WARN("DIAGNOSTIC MODE: frame hook only - reads and logs, never "
                    "writes a target. Verify the offsets from this log, then "
                    "set Mode=live.");
    } else { // RunMode::Live
        if (!SelectionHook::Install())
            AT_LOG_WARN("SelectionHook unavailable - manual-hold detection disabled");
        if (g_config.Get().mechanism == Mechanism::ActionTarget) {
            if (!SpellCastHook::Install(g_controller))
                AT_LOG_WARN("SpellCastHook unavailable - ActionTarget commits "
                            "disabled, falling back to mouseover behaviour");
            if (!TabHook::Install(g_controller))
                AT_LOG_WARN("TabHook unavailable - Tab keeps native "
                            "cycle-by-proximity behaviour");
        }
    }

    bool framed = false;
    for (int attempt = 0; attempt < kFrameHookAttempts; ++attempt) {
        if (FrameHook::Install(&OnFrame)) {
            framed = true;
            break;
        }
        Sleep(kFrameHookRetryMs);
    }
    if (!framed) {
        AT_LOG_ERROR("AutoTarget self-disabled: could not hook the frame loop");
        return 0;
    }

    AT_LOG_INFO("AutoTarget ready (enabled=%s)",
                g_controller->IsEnabled() ? "true" : "false");
    return 0;
}

} // namespace

void Begin(HMODULE self) {
    // Pin our own module so Windows can never unmap it for the rest of the
    // process. WoW imports version.dll only for a handful of GetFileVersionInfo*
    // calls and may release its reference shortly after start; without pinning,
    // our image could be unloaded while our hook detours (which live inside
    // that image) are still installed in the client, and the next frame jumps
    // into freed memory and crashes. Pinning also means the module attaches
    // exactly once, so initialization never doubles up.
    HMODULE pinned = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN |
                           GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCSTR>(&Begin), &pinned);

    // Belt and braces: initialize exactly once even if attach somehow repeats.
    static volatile LONG started = 0;
    if (InterlockedExchange(&started, 1) != 0)
        return;

    g_self = self;
    const HANDLE thread = CreateThread(nullptr, 0, &InitThread, nullptr, 0, nullptr);
    if (thread != nullptr)
        CloseHandle(thread); // detach; the thread runs to completion on its own
}

void End(bool processTerminating) {
    // Stop invoking our callback so no tick runs during teardown.
    FrameHook::Uninstall();
    SelectionHook::Uninstall();
    SpellCastHook::Uninstall();
    TabHook::Uninstall();

    if (!processTerminating) {
        // A real FreeLibrary unload: remove detours so we leave clean.
        MH_Uninitialize();
        delete g_controller;
        g_controller = nullptr;
    }
    // On process termination the OS reclaims everything; touching MinHook or the
    // heap under the exit-time loader lock is riskier than just leaving it.

    AT_LOG_INFO("AutoTarget shutting down");
    Logger::Shutdown();
}

} // namespace Initializer
} // namespace autotarget
