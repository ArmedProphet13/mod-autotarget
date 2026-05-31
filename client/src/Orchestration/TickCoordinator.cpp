#include "Orchestration/TickCoordinator.h"

#include <cmath>

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "Diagnostics/SafeMode.h"
#include "Engine/Mechanisms/ActionTargetHandler.h"
#include "Engine/Mechanisms/HardTargetHandler.h"
#include "Engine/Mechanisms/MouseoverHandler.h"
#include "Engine/SpellCommitPolicy.h"
#include "Engine/UnstickPolicy.h"
#include "GameInterface/Camera.h"
#include "GameInterface/LineOfSight.h"
#include "GameInterface/Offsets.h"
#include "GameInterface/Selection.h"
#include "GameInterface/Hooks/FrameHook.h"
#include "GameInterface/Hooks/SelectionHook.h"

namespace {

std::unique_ptr<autotarget::IMechanismHandler> MakeHandler(
    autotarget::Mechanism m, autotarget::ISelectionSink& sink) {
    using namespace autotarget;
    switch (m) {
        case Mechanism::ActionTarget: return std::make_unique<ActionTargetHandler>(sink);
        case Mechanism::Mouseover:    return std::make_unique<MouseoverHandler>(sink);
        case Mechanism::HardTarget:
        default:                      return std::make_unique<HardTargetHandler>(sink);
    }
}

} // namespace

namespace autotarget {

TickCoordinator::TickCoordinator(const ConfigManager& config,
                                 const IToggleTarget& toggle)
    : cfg_(config.Get()),
      engine_(config.ToEngineConfig()),
      toggle_(toggle),
      handler_(MakeHandler(config.Get().mechanism, sink_)),
      diagnostic_(config.Get().mode == RunMode::Diagnostic),
      mechanism_(config.Get().mechanism),
      aimOffsetRad_(config.Get().aimOffsetDegrees * 3.14159265f / 180.0f),
      ignoreCritters_(config.Get().ignoreCritters),
      tickIntervalMs_(config.Get().tickRateMs) {
    LineOfSight::SetEnabled(cfg_.lineOfSightChecks);
}

void TickCoordinator::ApplyConfig(const ConfigManager& config) {
    cfg_ = config.Get();
    engine_.SetConfig(config.ToEngineConfig());
    tickIntervalMs_ = cfg_.tickRateMs;
    LineOfSight::SetEnabled(cfg_.lineOfSightChecks);
}

bool TickCoordinator::IsActiveTargetUnusableCheap(Guid guid) {
    if (guid == kNoGuid)
        return false;

    const WoWUnit u  = objMgr_.FindUnit(guid);
    const WoWUnit me = objMgr_.LocalPlayer();

    UnstickInputs in;
    in.activeExists      = u.Valid();
    if (in.activeExists) {
        in.activeAlive      = u.IsAlive();
        in.activeAttackable = u.IsAttackable();
        if (me.Valid()) {
            in.activeDx = u.X() - me.X();
            in.activeDy = u.Y() - me.Y();
            in.activeDz = u.Z() - me.Z();
        }
    }
    in.maxRangeYards = cfg_.smartUnstickMaxRangeYards;

    const UnstickReason r = EvaluateUnstickReasonLight(in);
    if (r == UnstickReason::None) return false;
    if (r == UnstickReason::Dead || r == UnstickReason::Missing) return true;
    return cfg_.smartUnstick;
}

void TickCoordinator::OnFrame() {
    if (!SafeMode::IsEnabled())
        return;

    // Per-frame mechanism writes (cursor-yield mouseover for ActionTarget,
    // unconditional re-pin for legacy Mouseover, no-op for HardTarget).
    MechanismCtx ctx{softTarget_, lastWrittenMouseover_,
                     toggle_.IsEnabled(), diagnostic_};
    handler_->OnFrame(ctx);
    lastWrittenMouseover_ = ctx.lastWrittenMouseover;

    const std::uint32_t now = GetTickCount();
    if (now - lastTickMs_ < tickIntervalMs_)
        return;
    lastTickMs_ = now;

    SafeMode::Run(&TickCoordinator::TickThunk, this, "coordinator tick");
}

void TickCoordinator::TickThunk(void* ctx) {
    static_cast<TickCoordinator*>(ctx)->Tick();
}

void TickCoordinator::Tick() {
    inWorld_ = objMgr_.Refresh() && objMgr_.InWorld();
    if (!inWorld_)
        return;

    const WoWUnit me = objMgr_.LocalPlayer();
    if (!me.Valid())
        return;

    TargetingSnapshot snap;
    snap.playerX       = me.X();
    snap.playerY       = me.Y();
    snap.playerZ       = me.Z();
    snap.cameraYaw     = Camera::AimYaw(me) + aimOffsetRad_;
    snap.inCombat      = (me.Flags() & offsets::kUnitFlagInCombat) != 0;
    snap.actionPending = false;
    snap.currentTarget = me.TargetGuid();
    snap.nowMs         = GetTickCount();

    Guid manualHold = SelectionHook::LastManualTarget();
    if (manualHold != kNoGuid) {
        const WoWUnit held = objMgr_.FindUnit(manualHold);
        if (!held.Valid() || !held.IsAlive()) {
            SelectionHook::ClearManual();
            manualHold = kNoGuid;
        }
    }
    snap.manualHold = manualHold;

    const Guid localGuid = objMgr_.LocalPlayerGuid();
    const float rangeSq  = cfg_.tier2RangeYards * cfg_.tier2RangeYards;

    objMgr_.ForEachUnit([&](const WoWUnit& u) {
        const Guid guid = u.ObjectGuid();
        if (guid == kNoGuid || guid == localGuid)
            return;

        const float dx = u.X() - snap.playerX;
        const float dy = u.Y() - snap.playerY;
        const float dz = u.Z() - snap.playerZ;
        if (guid != snap.currentTarget && dx * dx + dy * dy + dz * dz > rangeSq)
            return;

        UnitInfo info;
        info.guid          = guid;
        info.x             = u.X();
        info.y             = u.Y();
        info.z             = u.Z();
        info.alive         = u.IsAlive();
        info.attackable    = u.IsAttackable();
        info.critter       = ignoreCritters_ && u.IsCritter();
        info.lineOfSight   = LineOfSight::Visible(me, u);
        info.healthPercent = u.HealthPercent();
        snap.units.push_back(info);
    });

    const TargetingDecision decision = engine_.Evaluate(snap);
    softTarget_ = decision.softTarget;

    // SmartUnstick (v0.3.4) - see AutoTargetController history for the rationale.
    if (!diagnostic_ && toggle_.IsEnabled() && snap.currentTarget != kNoGuid) {
        const WoWUnit active = objMgr_.FindUnit(snap.currentTarget);

        UnstickInputs in;
        in.activeExists     = active.Valid();
        if (in.activeExists) {
            in.activeAlive      = active.IsAlive();
            in.activeAttackable = active.IsAttackable();
            in.activeDx         = active.X() - snap.playerX;
            in.activeDy         = active.Y() - snap.playerY;
            in.activeDz         = active.Z() - snap.playerZ;
            in.activeVisibleLoS = LineOfSight::Visible(me, active);
        }
        in.playerFacingRad  = snap.cameraYaw;
        in.maxRangeYards    = cfg_.smartUnstickMaxRangeYards;

        in.softPick = decision.softTarget;
        if (decision.softTarget != kNoGuid) {
            for (const auto& u : snap.units) {
                if (u.guid != decision.softTarget) continue;
                in.softDx = u.x - snap.playerX;
                in.softDy = u.y - snap.playerY;
                const float angleToSoft = std::atan2(in.softDy, in.softDx);
                float delta = angleToSoft - snap.cameraYaw;
                while (delta >  3.14159265f) delta -= 6.28318530f;
                while (delta < -3.14159265f) delta += 6.28318530f;
                in.softInCone = std::fabs(delta) <= 1.57079633f;
                break;
            }
        }

        UnstickReason r = EvaluateUnstickReasonLight(in);
        if (r != UnstickReason::None
            && r != UnstickReason::Dead
            && r != UnstickReason::Missing
            && !cfg_.smartUnstick) {
            r = UnstickReason::None;
        }
        if (r == UnstickReason::None && cfg_.smartUnstick) {
            r = EvaluateUnstickReason(in);
        }

        if (r != UnstickReason::None) {
            AT_LOG_DEBUG("unstick: %s active=%016llX",
                         ReasonName(r),
                         static_cast<unsigned long long>(snap.currentTarget));
            SelectionHook::BeginCommit();
            Selection::SetTarget(kNoGuid);
            SelectionHook::EndCommit();
        }
    }

    if (diagnostic_) {
        if (snap.nowMs - lastDiagMs_ >= 2000) {
            lastDiagMs_ = snap.nowMs;
            AT_LOG_INFO("diag: player=(%.1f,%.1f,%.1f) yaw=%.3f units=%d "
                        "combat=%d current=%016llX -> %s soft=%016llX",
                        snap.playerX, snap.playerY, snap.playerZ, snap.cameraYaw,
                        static_cast<int>(snap.units.size()),
                        snap.inCombat ? 1 : 0,
                        static_cast<unsigned long long>(snap.currentTarget),
                        decision.reason,
                        static_cast<unsigned long long>(decision.softTarget));
        }
        return;
    }

    // Periodic ActionTarget breadcrumb — the visible-ring + cast-hook path
    // never logs per-event, so this keeps the log alive in steady state.
    if (mechanism_ == Mechanism::ActionTarget
        && snap.nowMs - lastDiagMs_ >= 2000) {
        lastDiagMs_ = snap.nowMs;
        AT_LOG_DEBUG("action: units=%d combat=%d active=%016llX "
                     "soft=%016llX mouseover-slot=%016llX yaw=%.2f (%s)",
                     static_cast<int>(snap.units.size()),
                     snap.inCombat ? 1 : 0,
                     static_cast<unsigned long long>(snap.currentTarget),
                     static_cast<unsigned long long>(decision.softTarget),
                     static_cast<unsigned long long>(Selection::Mouseover()),
                     snap.cameraYaw, decision.reason);
    }

    // Mechanism-specific write decisions live in the strategy. Pass through
    // the live ctx so MouseoverHandler can update lastWrittenMouseover_.
    MechanismCtx ctx{softTarget_, lastWrittenMouseover_,
                     toggle_.IsEnabled(), diagnostic_};
    handler_->OnTickResult(ctx, decision);
    lastWrittenMouseover_ = ctx.lastWrittenMouseover;
}

} // namespace autotarget
