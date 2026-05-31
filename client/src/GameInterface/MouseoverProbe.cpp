#include "GameInterface/MouseoverProbe.h"

#include <cstdint>

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

namespace {

// WoW.exe code range. The client images at 0x00400000 and is ~7.7 MB; this
// brackets it generously while excluding our own DLL and the DXVK wrapper
// (both loaded far higher), so only the client's own code is reported.
constexpr std::uintptr_t kClientLo = 0x00400000;
constexpr std::uintptr_t kClientHi = 0x00D00000;

constexpr std::uint32_t kProbeWindowMs = 90000; // self-disarm after 90 s

PVOID          g_veh = nullptr;
bool           g_armed = false;
bool           g_disarmed = false;
std::uint32_t  g_armMs = 0;
std::uintptr_t g_seen[32];
int            g_seenCount = 0;

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;

    CONTEXT* ctx = ep->ContextRecord;
    const std::uintptr_t eip = ctx->Eip;

    // Only client code is interesting - skip our own writes and the wrapper.
    if (eip >= kClientLo && eip < kClientHi) {
        bool known = false;
        for (int i = 0; i < g_seenCount; ++i) {
            if (g_seen[i] == eip) {
                known = true;
                break;
            }
        }
        if (!known && g_seenCount < 32) {
            g_seen[g_seenCount++] = eip;
            AT_LOG_INFO("mouseoverprobe: client code at %08X touches the mouseover slot",
                        static_cast<unsigned>(eip));
        }
    }

    ctx->Dr6 = 0; // clear the debug status so the breakpoint keeps working

    if (!g_disarmed && GetTickCount() - g_armMs > kProbeWindowMs) {
        g_disarmed = true;
        ctx->Dr7 = 0; // disarm every hardware breakpoint
        AT_LOG_INFO("mouseoverprobe: finished - %d distinct client site(s) found",
                    g_seenCount);
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

} // namespace

void MouseoverProbe::Arm() {
    if (g_armed)
        return;
    g_armed = true;
    g_armMs = GetTickCount();

    g_veh = AddVectoredExceptionHandler(1, &Veh);
    if (g_veh == nullptr) {
        AT_LOG_WARN("mouseoverprobe: AddVectoredExceptionHandler failed");
        return;
    }

    // Hardware breakpoint on Dr0: break on read or write of 4 bytes at the
    // mouseover GUID slot. DR7 = L0 | (RW0=11 << 16) | (LEN0=11 << 18).
    CONTEXT ctx;
    ZeroMemory(&ctx, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    ctx.Dr0 = offsets::kStaticMouseoverGuid;
    ctx.Dr7 = 0x000F0001;

    if (SetThreadContext(GetCurrentThread(), &ctx)) {
        AT_LOG_INFO("mouseoverprobe: armed on slot %08X for %u s",
                    static_cast<unsigned>(offsets::kStaticMouseoverGuid),
                    kProbeWindowMs / 1000);
    } else {
        AT_LOG_WARN("mouseoverprobe: SetThreadContext failed (%lu)", GetLastError());
    }
}

} // namespace autotarget
