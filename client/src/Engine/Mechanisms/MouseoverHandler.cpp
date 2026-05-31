#include "Engine/Mechanisms/MouseoverHandler.h"

namespace autotarget {

void MouseoverHandler::OnFrame(MechanismCtx& ctx) {
    if (ctx.diagnostic || !ctx.enabled)
        return;
    sink_.WriteMouseover(ctx.lastWrittenMouseover);
}

void MouseoverHandler::OnTickResult(MechanismCtx& ctx,
                                    const TargetingDecision& decision) {
    if (ctx.diagnostic)
        return;
    ctx.lastWrittenMouseover = ctx.enabled ? decision.softTarget : kNoGuid;
}

} // namespace autotarget
