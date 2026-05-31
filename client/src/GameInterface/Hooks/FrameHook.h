#pragma once

#include <functional>

namespace autotarget {

// Hooks Direct3D9 EndScene so a callback runs once per frame on the game
// thread. On the single-threaded 3.3.5 client EndScene is called on the main
// thread, which is exactly where it is safe to touch client state.
class FrameHook {
public:
    using Callback = std::function<void()>;

    // Installs the hook. Returns false if the device could not be located.
    // Requires MinHook to already be initialized.
    static bool Install(Callback onFrame);

    // Stops invoking the callback. Actual detour removal happens via
    // MH_Uninitialize during shutdown.
    static void Uninstall();

    // The Direct3D 9 device pointer seen on the most recent frame (nullptr
    // before the first frame). Only valid to use on the game thread.
    static void* Device();

private:
    FrameHook() = default;
};

} // namespace autotarget
