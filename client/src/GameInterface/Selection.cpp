#include "GameInterface/Selection.h"

#include <cstdint>

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "GameInterface/FrameScript.h"
#include "GameInterface/Memory.h"
#include "GameInterface/Offsets.h"

namespace autotarget {
namespace Selection {

namespace {
// CGGameUI::Target(guid). Calling convention is a VERIFY item — if a mismatch
// corrupts the stack in-client, this typedef is the place to correct it.
using TargetUnitFn   = void(__cdecl*)(std::uint64_t guid);
// CGGameUI::Set_Mouseover(guid). Same shape; see Offsets.h for the VERIFY
// notes on kFnSetMouseover.
using SetMouseoverFn = void(__cdecl*)(std::uint64_t guid);

// Sticky: if a call to the client's Set_Mouseover ever faults (wrong offset,
// wrong calling convention) we permanently fall back to raw-slot-only writes.
// One log line, no further attempts, no further faults. The functional
// behaviour (slot is set, [target=mouseover] macros work) is preserved by the
// raw write that always runs first.
bool g_highlightDisabled = false;

// Candidate Set_Mouseover address. Defaults to the Offsets.h value; the INI can
// override it (Selection::SetMouseoverOffset) so offsets are probed without a
// rebuild. 0 means "don't call the native routine".
std::uintptr_t g_mouseoverFn   = offsets::kFnSetMouseover;
bool           g_tooltipViaLua  = false;
Guid           g_lastProbeGuid  = kNoGuid; // throttles the probe diagnostic

// Returns true if the native call ran without faulting (or was skipped because
// disabled); false only when it faulted this call. Used by the probe diag.
bool TryCallClientMouseover(std::uint64_t guid) {
    if (g_highlightDisabled || g_mouseoverFn == 0)
        return true;
    const auto fn = reinterpret_cast<SetMouseoverFn>(g_mouseoverFn);
    __try {
        fn(guid);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_highlightDisabled = true;
        AT_LOG_WARN("Selection: Set_Mouseover @%p faulted - native highlight "
                    "call disabled (raw slot write continues, macros unaffected)",
                    reinterpret_cast<void*>(g_mouseoverFn));
        return false;
    }
}

// Tier-2 fallback: drive the game's own shared GameTooltip via Lua. Addon-safe,
// tooltip only (no world ring). The mouseover slot is already raw-written, so
// SetUnit("mouseover") resolves to the soft pick.
void TryTooltipViaLua(std::uint64_t guid) {
    if (!g_tooltipViaLua)
        return;
    // Throttle: only re-run the Lua when the soft pick changes, so we don't
    // re-parse a string every frame. Re-affirming SetUnit on the same pick adds
    // nothing - the tooltip already shows it.
    static Guid s_lastLuaGuid = ~Guid(0); // sentinel so the first call always runs
    if (guid == s_lastLuaGuid)
        return;
    s_lastLuaGuid = guid;

    if (guid == kNoGuid) {
        FrameScript::Execute("GameTooltip:Hide()");
    } else {
        // GameTooltip_SetDefaultAnchor places the tooltip exactly where the
        // native hover tooltip goes (bottom-right by default) AND is the very
        // function tooltip-mover addons hook - so addons that reposition the
        // tooltip will reposition ours too. The mouseover slot is already
        // raw-written, so "mouseover" resolves to the soft pick.
        FrameScript::Execute(
            "GameTooltip_SetDefaultAnchor(GameTooltip, UIParent); "
            "GameTooltip:SetUnit(\"mouseover\"); GameTooltip:Show()");
    }
}
} // namespace

void SetMouseoverOffset(std::uintptr_t addr) {
    g_mouseoverFn = addr;
    AT_LOG_INFO("Selection: Set_Mouseover offset = %p%s",
                reinterpret_cast<void*>(addr),
                addr == 0 ? " (native call disabled)" : "");
}

void SetTooltipViaLua(bool on) {
    g_tooltipViaLua = on;
    if (on)
        AT_LOG_INFO("Selection: Tier-2 GameTooltip:SetUnit fallback enabled");
}

Guid CurrentTarget() {
    return mem::ReadOr<Guid>(offsets::kStaticTargetGuid, kNoGuid);
}

Guid Mouseover() {
    return mem::ReadOr<Guid>(offsets::kStaticMouseoverGuid, kNoGuid);
}

void SetTarget(Guid guid) {
    const auto fn = reinterpret_cast<TargetUnitFn>(offsets::kFnTargetUnit);
    fn(guid);
}

void SetMouseover(Guid guid) {
    // Raw slot write first: this is the guaranteed-working path. It makes
    // [target=mouseover] macros resolve to the soft pick and is what kept
    // v0.3.0-v0.3.2 functional even without the visual highlight.
    mem::Write<Guid>(offsets::kStaticMouseoverGuid, guid);

    // Then call the client's own mouseover routine to drive the yellow
    // selection-ring highlight + tooltip. Wrapped in SEH so a wrong offset
    // can't crash the client; on first fault we latch off and keep the raw
    // write only. Cursor priority is preserved by the caller (the controller
    // never calls us while the cursor is asserting a unit).
    const bool ok = TryCallClientMouseover(guid);

    // Probe diagnostic: on each distinct soft-pick change, report what the
    // native call did so the user can confirm in-client whether the tooltip +
    // ring fired, did nothing, or faulted. Throttled to pick changes so the
    // log isn't flooded at frame rate. INFO-level so it's visible at default
    // verbosity while probing.
    if (guid != g_lastProbeGuid) {
        g_lastProbeGuid = guid;
        AT_LOG_INFO("Selection: mouseover probe pick=%016llX fn=%p called=%d "
                    "faulted=%d slot=%016llX",
                    static_cast<unsigned long long>(guid),
                    reinterpret_cast<void*>(g_mouseoverFn),
                    (g_mouseoverFn != 0 && !g_highlightDisabled) ? 1 : 0,
                    ok ? 0 : 1,
                    static_cast<unsigned long long>(Mouseover()));
    }

    // Tier-2: optionally drive the real GameTooltip via Lua (tooltip only).
    TryTooltipViaLua(guid);
}

} // namespace Selection
} // namespace autotarget
