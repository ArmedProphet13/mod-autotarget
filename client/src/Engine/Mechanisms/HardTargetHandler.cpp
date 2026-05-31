#include "Engine/Mechanisms/HardTargetHandler.h"

#include "Diagnostics/Logger.h"

namespace autotarget {

void HardTargetHandler::OnTickResult(MechanismCtx& ctx,
                                     const TargetingDecision& decision) {
    if (ctx.diagnostic || !ctx.enabled)
        return;
    if (decision.kind != DecisionKind::SetHardTarget)
        return;

    sink_.BeginActiveTargetCommit();
    sink_.WriteActiveTarget(decision.hardTarget);
    sink_.EndActiveTargetCommit();

    AT_LOG_DEBUG("commit hard target %016llX (%s)",
                 static_cast<unsigned long long>(decision.hardTarget),
                 decision.reason);
}

} // namespace autotarget
