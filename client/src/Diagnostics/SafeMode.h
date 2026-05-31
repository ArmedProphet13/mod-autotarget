#pragma once

namespace autotarget {

// Crash containment.
//
// Every hook callback and the per-tick body runs through SafeMode::Run, which
// catches structured exceptions (access violations and the like). A fault
// disables AutoTarget cleanly and is logged — it never crashes WoW.exe.
namespace SafeMode {

// Guarded bodies are plain function pointers (no captures) plus a context
// pointer, so SafeMode::Run can wrap them in a structured-exception handler
// without object-unwinding conflicts.
using GuardedFn = void (*)(void* ctx);

bool        IsEnabled();
void        Enable();
void        Disable(const char* reason);
const char* DisableReason();

// Runs fn(ctx) under a structured-exception guard. Returns true if it
// completed normally, false if it faulted (in which case AutoTarget is
// disabled and the fault is logged).
bool Run(GuardedFn fn, void* ctx, const char* what);

} // namespace SafeMode
} // namespace autotarget
