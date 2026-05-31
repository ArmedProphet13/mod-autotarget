#include "GameInterface/Selection.h"

#include <cstdint>

#include <windows.h>

#include "Diagnostics/Logger.h"
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

void TryCallClientMouseover(std::uint64_t guid) {
    if (g_highlightDisabled || offsets::kFnSetMouseover == 0)
        return;
    const auto fn = reinterpret_cast<SetMouseoverFn>(offsets::kFnSetMouseover);
    __try {
        fn(guid);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_highlightDisabled = true;
        AT_LOG_WARN("Selection: kFnSetMouseover faulted - highlight call "
                    "disabled (raw slot write continues, macros unaffected)");
    }
}
} // namespace

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
    TryCallClientMouseover(guid);
}

} // namespace Selection
} // namespace autotarget
