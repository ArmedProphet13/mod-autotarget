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
    if (cursorAsserting || ctx.softTarget == kNoGuid)
        return;

    sink_.WriteMouseover(ctx.softTarget);
    ctx.lastWrittenMouseover = ctx.softTarget;
}

void ActionTargetHandler::OnTickResult(MechanismCtx& /*ctx*/,
                                       const TargetingDecision& /*decision*/) {
    // Active-target writes happen in SpellCastHook / TabHook only.
}

} // namespace autotarget
