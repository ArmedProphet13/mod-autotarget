#include "Engine/Mechanisms/ActionTargetHandler.h"

namespace autotarget {

void ActionTargetHandler::OnFrame(MechanismCtx& ctx) {
    if (ctx.diagnostic || !ctx.enabled)
        return;

    // Cursor yield: if the mouseover slot holds a guid that is not the value
    // we last wrote, the cursor is asserting and reactive macros must win.
    const Guid cursor = sink_.ReadMouseover();
    const bool cursorAsserting = (cursor != kNoGuid)
                              && (cursor != ctx.lastWrittenMouseover);
    if (cursorAsserting)
        return;

    // Nothing aimed: release the slot if we are the one holding it. This clears
    // [@mouseover] and hides the soft-target tooltip the moment aim is lost,
    // matching native hover behaviour (no aim -> no mouseover). Guarded on
    // lastWrittenMouseover so we don't re-write kNoGuid every idle frame.
    if (ctx.softTarget == kNoGuid) {
        if (ctx.lastWrittenMouseover != kNoGuid) {
            sink_.WriteMouseover(kNoGuid);
            ctx.lastWrittenMouseover = kNoGuid;
        }
        return;
    }

    sink_.WriteMouseover(ctx.softTarget);
    ctx.lastWrittenMouseover = ctx.softTarget;
}

void ActionTargetHandler::OnTickResult(MechanismCtx& /*ctx*/,
                                       const TargetingDecision& /*decision*/) {
    // Active-target writes happen in SpellCastHook / TabHook only.
}

} // namespace autotarget
