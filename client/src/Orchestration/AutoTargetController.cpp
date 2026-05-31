#include "Orchestration/AutoTargetController.h"

#include <cmath>

#include <windows.h>

#include "Diagnostics/Logger.h"
#include "Diagnostics/SafeMode.h"
#include "Engine/SpellCommitPolicy.h"
#include "Engine/UnstickPolicy.h"
#include "GameInterface/Camera.h"
#include "GameInterface/LineOfSight.h"
#include "GameInterface/Offsets.h"
#include "GameInterface/Selection.h"
#include "GameInterface/Hooks/FrameHook.h"
#include "GameInterface/Hooks/SelectionHook.h"

namespace autotarget {

AutoTargetController::AutoTargetController(const ConfigManager& config)
    : cfg_(config.Get()),
      engine_(config.ToEngineConfig()),
      enabled_(config.Get().enabledOnStartup),
      diagnostic_(config.Get().mode == RunMode::Diagnostic),
      mechanism_(config.Get().mechanism),
      aimOffsetRad_(config.Get().aimOffsetDegrees * 3.14159265f / 180.0f),
      ignoreCritters_(config.Get().ignoreCritters),
      tickIntervalMs_(config.Get().tickRateMs) {}

void AutoTargetController::ApplyConfig(const ConfigManager& config) {
    cfg_ = config.Get();
    engine_.SetConfig(config.ToEngineConfig());
    tickIntervalMs_ = cfg_.tickRateMs;
}

void AutoTargetController::SetEnabled(bool on) {
    if (on == enabled_)
        return;
    enabled_ = on;
    AT_LOG_INFO("AutoTarget %s", on ? "enabled" : "disabled");
}

bool AutoTargetController::IsActiveTargetUnusableCheap(Guid guid) {
    if (guid == kNoGuid)
        return false;

    // Assemble the LIGHT-only UnstickInputs. The cast hook runs on the game
    // thread so the object list is in a consistent state - the same place
    // the v0.3.2/v0.3.3 corpse predicate ran. We deliberately do NOT touch
    // soft-pick / facing here; that needs a snapshot and lives in Tick().
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

    // Dead/Missing are unconditional; the rest are gated by the feature flag.
    if (r == UnstickReason::Dead || r == UnstickReason::Missing) return true;
    return cfg_.smartUnstick;
}

void AutoTargetController::OnFrame() {
    // A prior fault latched SafeMode off — never touch the client again.
    if (!SafeMode::IsEnabled())
        return;

    // Mouseover writer with cursor yield. In ActionTarget mode we drive the
    // mouseover slot with the engine's soft pick so the player sees a yellow
    // highlight ring + tooltip on the enemy AutoTarget is aiming at. BUT we
    // must never fight the player's cursor: if the cursor is currently over
    // a unit (the client has written that unit's guid into the mouseover
    // slot, and the guid is not the value we last wrote), we yield this
    // frame so reactive [target=mouseover] macros (interrupts, heals) work
    // unaltered.
    //
    // EndScene runs at the very end of each frame, after the client has had
    // a chance to write its own mouseover from cursor hover, so the value we
    // read here reflects the cursor's intent.
    if (!diagnostic_ && enabled_ && mechanism_ == Mechanism::ActionTarget) {
        const Guid cursor = Selection::Mouseover();
        const bool cursorAsserting = (cursor != kNoGuid)
                                  && (cursor != lastWrittenMouseover_);
        // Re-assert the soft pick every frame when not yielding, even if it
        // matches lastWritten. The client can clear the mouseover slot on its
        // own (cursor briefly hovering then leaving a unit empties it to 0),
        // and a change-gated writer would never re-pin our value in that case
        // - the highlight would blink out during stable aim. Writing every
        // frame is cheap (one 64-bit memory store) and keeps the ring on.
        if (!cursorAsserting && softTarget_ != kNoGuid) {
            Selection::SetMouseover(softTarget_);
            lastWrittenMouseover_ = softTarget_;
        }
    } else if (!diagnostic_ && enabled_ && mechanism_ == Mechanism::Mouseover) {
        // Legacy mouseover mechanism: drive the slot unconditionally
        // (no cursor yield). Kept for fallback.
        Selection::SetMouseover(lastWrittenMouseover_);
    }

    const std::uint32_t now = GetTickCount();
    if (now - lastTickMs_ < tickIntervalMs_)
        return;
    lastTickMs_ = now;

    SafeMode::Run(&AutoTargetController::TickThunk, this, "controller tick");
}

void AutoTargetController::TickThunk(void* ctx) {
    static_cast<AutoTargetController*>(ctx)->Tick();
}

void AutoTargetController::Tick() {
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
    snap.actionPending = false; // commit-on-action (SpellCastHook) is future scope
    // The local player's own descriptor field is the authoritative current
    // target (a CONFIRMED offset), and unlike the static it always reflects a
    // target we set ourselves.
    snap.currentTarget = me.TargetGuid();
    snap.nowMs         = GetTickCount();

    // A manual pick holds until the picked unit dies or is cleared. Drop a stale
    // hold here so aggressive re-aim resumes the moment that unit is gone.
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
        // Drop units beyond the widest tier - but always keep the current hard
        // target so the state machine can still see it (and not mistake a
        // chased, lagging target for "no target" and re-commit every tick).
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

    // SmartUnstick (v0.3.4) - clear the active target slot when its OWN guid
    // is demonstrably unusable: dead, despawned, out of cast range,
    // server-leashed (evading), or sitting >90 deg behind the player while a
    // closer enemy is in the aim cone. Each reason names itself in the log.
    //
    // Hard rule: we ONLY ever inspect snap.currentTarget. Other corpses,
    // other unreachable mobs, anything else in the world is none of our
    // business. The slot's own contents are the only thing we act on.
    if (!diagnostic_ && enabled_ && snap.currentTarget != kNoGuid) {
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

        // Heavy inputs: soft pick geometry (only used by BehindSoftCloser /
        // OutOfLoS). softInCone = "soft is in the front half-arc" as a
        // generous proxy for the engine's tier cone gating; the precise
        // angle isn't needed because the BehindSoftCloser check itself
        // requires the ACTIVE to be >90 deg off-facing.
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

        // Dead/Missing always fire. Other reasons gated by SmartUnstick.
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

    // Diagnostic mode: log what the engine would do, but never touch the client.
    // This exercises every read offset so the log can confirm the [VERIFY]
    // values, with no risk of a crash.
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

    // ActionTarget mode does no target-slot writing from Tick(). The actual
    // mouseover-slot write happens in OnFrame() (with cursor yield), which
    // runs every rendered frame instead of every tick. Target-slot writes
    // only happen in response to player explicit input: SpellCastHook on
    // an offensive cast with empty slot, or TabHook on Tab press.
    //
    // The in-combat tick-promote from v0.2.2 has been removed: the
    // always-on mouseover ring already shows the next victim the instant
    // the previous target dies; the player explicitly commits via cast
    // or Tab. AutoTarget never silently changes the active target slot.
    if (mechanism_ == Mechanism::ActionTarget) {
        if (snap.nowMs - lastDiagMs_ >= 2000) {
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
        return;
    }

    // Legacy mouseover mechanism (Mechanism::Mouseover): drives the slot
    // unconditionally, no cursor yield. Kept for fallback.
    if (mechanism_ == Mechanism::Mouseover) {
        lastWrittenMouseover_ = enabled_ ? decision.softTarget : kNoGuid;
        return;
    }

    if (decision.kind != DecisionKind::SetHardTarget)
        return;
    if (!enabled_)
        return; // soft tracking stays warm; no hard write while disabled

    // Bracket our own write so SelectionHook does not mistake it for a manual
    // pick (which would immediately latch a re-aim-blocking hold).
    SelectionHook::BeginCommit();
    Selection::SetTarget(decision.hardTarget);
    SelectionHook::EndCommit();

    AT_LOG_DEBUG("commit hard target %016llX (%s)",
                 static_cast<unsigned long long>(decision.hardTarget),
                 decision.reason);
}

} // namespace autotarget
