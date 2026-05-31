#include "GameInterface/FrameScript.h"

#include <cstdint>

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "GameInterface/Offsets.h"

namespace autotarget {
namespace FrameScript {

namespace {
using ExecuteFn  = void(__cdecl*)(const char* code, const char* source, int unused);
using RegisterFn = void(__cdecl*)(const char* name, void* func);

// Sticky: if a call to FrameScript_RegisterFunction ever faults (wrong offset
// or wrong calling convention) we permanently fall back to "no bridge". One
// log line, no further attempts. The mod stays loaded and functional; only
// the in-game checkbox + /at slash command are absent.
bool g_registerDisabled = false;
} // namespace

void Execute(const char* luaCode) {
    if (luaCode == nullptr)
        return;
    const auto fn = reinterpret_cast<ExecuteFn>(offsets::kFnFrameScriptExecute);
    fn(luaCode, "AutoTarget", 0);
}

bool RegisterFunction(const char* name, LuaCFunction fn) {
    if (g_registerDisabled
        || offsets::kFnFrameScriptRegisterFunction == 0
        || name == nullptr
        || fn == nullptr)
        return false;
    const auto reg =
        reinterpret_cast<RegisterFn>(offsets::kFnFrameScriptRegisterFunction);
    __try {
        reg(name, reinterpret_cast<void*>(fn));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_registerDisabled = true;
        AT_LOG_WARN("FrameScript::RegisterFunction faulted at %p - "
                    "in-game checkbox + /at command disabled. Offset "
                    "kFnFrameScriptRegisterFunction is wrong on this client.",
                    reinterpret_cast<void*>(offsets::kFnFrameScriptRegisterFunction));
        return false;
    }
}

} // namespace FrameScript
} // namespace autotarget
