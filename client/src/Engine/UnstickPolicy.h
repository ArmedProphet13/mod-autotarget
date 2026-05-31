#pragma once

#include "Engine/EngineTypes.h"

// Pure-logic decision: is the player's CURRENT active target slot still usable?
//
// "Unusable" is not a guess at player intent - it is a deterministic predicate
// over plain inputs. Each non-None reason corresponds to a concrete situation
// where the cast / spell cannot land on the held GUID (dead, despawned, out of
// max cast range, server-leashed/evading) OR the player has unambiguously
// moved on (turned >90 degrees away while a closer enemy enters the aim cone).
//
// The controller proactively clears the slot on any non-None reason. The cast
// hook uses the LIGHT subset as a fast cross-check so the player isn't locked
// out for the ~70ms between ticks.
//
// This file deliberately has zero client-memory dependencies so the policy is
// unit-testable off-client.

namespace autotarget {

enum class UnstickReason {
    None,
    Dead,             // active is in objMgr but Health == 0 (corpse in slot)
    Missing,          // active GUID is not in the object manager at all
    OutOfRange,       // active is farther than maxRangeYards (default 40 yd)
    Evading,          // active is alive but no longer attackable (server leash)
    BehindSoftCloser, // active is >90 deg off facing AND a soft pick exists in
                      // the aim cone AND that soft pick is closer than active
    OutOfLoS          // active is not visible from the player. v1 LoS is
                      // permissive (always visible) so this is scaffolded but
                      // currently never fires in-client.
};

const char* ReasonName(UnstickReason r);

// Plain input bundle - assembled by the controller from the snapshot + a
// lookup of the active unit. No pointers, no client reads from this file.
struct UnstickInputs {
    // Active-target state. activeExists == false means GUID not found in the
    // object manager - i.e. Missing.
    bool  activeExists      = false;
    bool  activeAlive       = false;
    bool  activeAttackable  = false;
    float activeDx          = 0.0f; // active - player, world coordinates
    float activeDy          = 0.0f;
    float activeDz          = 0.0f;
    bool  activeVisibleLoS  = true; // see LineOfSight::Visible - v1 always true

    // Player state.
    float playerFacingRad   = 0.0f; // radians, atan2 convention (0 == +X)

    // Engine's current soft pick (if any). Distances are XY-only because the
    // "closer than active" comparison is for horizontal pick swap.
    Guid  softPick          = kNoGuid;
    float softDx            = 0.0f;
    float softDy            = 0.0f;
    bool  softInCone        = false; // true if the engine placed soft in the aim cone

    // Knobs.
    float maxRangeYards     = 40.0f;
};

// Full predicate: light + heavy. Returns the FIRST reason that fires (so the
// log line names the most specific cause); evaluation order is the order
// listed in the enum.
UnstickReason EvaluateUnstickReason(const UnstickInputs& in);

// Light-only subset: Dead | Missing | OutOfRange | Evading | None. Cheap
// enough to call on the cast key-press synchronously without snapshotting.
UnstickReason EvaluateUnstickReasonLight(const UnstickInputs& in);

} // namespace autotarget
