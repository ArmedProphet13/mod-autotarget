#include "SafeMode.h"

#include <windows.h>

#include "Logger.h"

namespace autotarget {
namespace SafeMode {

namespace {
bool        g_enabled = true;
const char* g_reason = "";
} // namespace

bool IsEnabled() {
    return g_enabled;
}

void Enable() {
    g_enabled = true;
    g_reason = "";
}

void Disable(const char* reason) {
    g_enabled = false;
    g_reason = reason ? reason : "unknown";
}

const char* DisableReason() {
    return g_reason;
}

bool Run(GuardedFn fn, void* ctx, const char* what) {
    if (!g_enabled || fn == nullptr)
        return false;

    __try {
        fn(ctx);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        const DWORD code = GetExceptionCode();
        Disable(what);
        Logger::Write(LogLevel::Error,
                      "SafeMode: structured exception in '%s' (code 0x%08lX) "
                      "- AutoTarget disabled",
                      what ? what : "?", code);
        return false;
    }
}

} // namespace SafeMode
} // namespace autotarget
