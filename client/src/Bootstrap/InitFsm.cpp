#include "Bootstrap/InitFsm.h"
#include "Bootstrap/Version.h"

#include <string>

#include "MinHook.h"

#include "Diagnostics/Logger.h"
#include "Diagnostics/SafeMode.h"
#include "GameInterface/Hooks/FrameHook.h"
#include "Input/EnableToggle.h"
#include "Orchestration/AutoTargetController.h"

namespace autotarget {

// Bridge global — FrameHook takes a plain function pointer, so we route via
// this single slot. Set by Start(), cleared by Stop().
static InitFsm* g_fsmForFrame = nullptr;

namespace {

void FrameStepThunk(void* ctx) {
    static_cast<InitFsm*>(ctx)->OnFrame();
}

} // namespace

void InitFsm::FrameCallback() {
    if (g_fsmForFrame != nullptr)
        SafeMode::Run(&FrameStepThunk, g_fsmForFrame, "frame");
}

namespace {

// WoW.exe build 12340 always images here. Every offset in Offsets.h is anchored
// to this base, so a mismatch means we are not in the supported client.
constexpr std::uintptr_t kExpectedImageBase = 0x00400000;

constexpr std::uint32_t kUiInstallDelayMs   = 5000;  // let FrameXML finish loading
constexpr int           kFrameHookAttempts  = 60;    // x 500ms = 30s for the device
constexpr DWORD         kFrameHookRetryMs   = 500;

std::string ModuleDirectory(HMODULE self) {
    char path[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameA(self, path, MAX_PATH);
    std::string s(path, n);
    const size_t slash = s.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : s.substr(0, slash + 1);
}

bool VerifyClient() {
    const HMODULE exe = GetModuleHandleA(nullptr);
    if (reinterpret_cast<std::uintptr_t>(exe) != kExpectedImageBase) {
        AT_LOG_ERROR("VerifyClient: unexpected image base %p (want %p)",
                     exe, reinterpret_cast<void*>(kExpectedImageBase));
        return false;
    }
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string name(exePath);
    const size_t slash = name.find_last_of("\\/");
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    AT_LOG_INFO("VerifyClient: host exe = '%s' (image base OK)", name.c_str());
    return true;
}

} // namespace

bool InitFsm::Start(HMODULE self) {
    self_ = self;
    const std::string dir = ModuleDirectory(self_);

    Logger::Init(dir + "AutoTarget.log", LogLevel::Info);
    AT_LOG_INFO("AutoTarget v%s starting up (pid=%lu, module=%p)",
                AUTOTARGET_VERSION_STRING, GetCurrentProcessId(), self_);

    if (!VerifyClient()) {
        AT_LOG_ERROR("AutoTarget self-disabled: unsupported client");
        state_ = State::Disabled;
        return false;
    }

    config_.Load(dir + "AutoTarget.ini");
    Logger::SetLevel(config_.Get().logLevel);
    state_ = State::ConfigLoaded;

    const RunMode mode = config_.Get().mode;
    live_ = (mode == RunMode::Live);

    if (mode == RunMode::Off) {
        AT_LOG_WARN("Mode=off: AutoTarget is loaded and forwarding graphics, "
                    "but installs no hook. Set Mode=diagnostic to begin.");
        state_ = State::Disabled;
        return true;
    }

    if (MH_Initialize() != MH_OK) {
        AT_LOG_ERROR("AutoTarget self-disabled: MH_Initialize failed");
        state_ = State::Disabled;
        return false;
    }

    controller_ = std::make_unique<AutoTargetController>(config_);

    if (mode == RunMode::Diagnostic) {
        AT_LOG_WARN("DIAGNOSTIC MODE: frame hook only - reads and logs, never "
                    "writes a target. Verify the offsets from this log, then "
                    "set Mode=live.");
    } else {
        ITargetingOracle* oracle =
            (config_.Get().mechanism == Mechanism::ActionTarget)
                ? &controller_->Oracle()
                : nullptr;
        hooks_.InstallLive(oracle);
    }
    state_ = State::ControllerBuilt;

    g_fsmForFrame = this;

    bool framed = false;
    for (int attempt = 0; attempt < kFrameHookAttempts; ++attempt) {
        if (FrameHook::Install(&InitFsm::FrameCallback)) {
            framed = true;
            break;
        }
        Sleep(kFrameHookRetryMs);
    }
    if (!framed) {
        AT_LOG_ERROR("AutoTarget self-disabled: could not hook the frame loop");
        state_ = State::Disabled;
        return false;
    }
    state_ = State::WaitingForWorld;

    AT_LOG_INFO("AutoTarget ready (enabled=%s)",
                controller_->IsEnabled() ? "true" : "false");
    return true;
}

void InitFsm::Stop(bool processTerminating) {
    FrameHook::Uninstall();
    hooks_.UninstallAll();
    g_fsmForFrame = nullptr;

    if (!processTerminating) {
        MH_Uninitialize();
        controller_.reset();
    }

    AT_LOG_INFO("AutoTarget shutting down");
    Logger::Shutdown();
}

void InitFsm::OnFrame() {
    if (controller_ == nullptr)
        return;

    if (live_ && state_ == State::WaitingForWorld
        && controller_->World().InWorld()) {
        const std::uint32_t now = GetTickCount();
        if (inWorldSinceMs_ == 0)
            inWorldSinceMs_ = now;
        if (now - inWorldSinceMs_ > kUiInstallDelayMs) {
            EnableToggle::Install(&controller_->Toggle());
            state_ = State::UiInstalled;
        }
    }

    controller_->OnFrame();
}

} // namespace autotarget
