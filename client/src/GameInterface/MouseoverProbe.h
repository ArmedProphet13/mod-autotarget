#pragma once

namespace autotarget {

// One-shot diagnostic. Sets a hardware breakpoint on the client's mouseover
// GUID slot and logs which client-code instructions read or write it. That
// reveals the function to hook so AutoTarget's soft target can own the slot
// outright instead of racing the client's cursor hit-test.
//
// Temporary tooling: it disarms itself after a fixed window.
class MouseoverProbe {
public:
    // Must be called on the game thread (the thread that touches the slot).
    static void Arm();

private:
    MouseoverProbe() = default;
};

} // namespace autotarget
