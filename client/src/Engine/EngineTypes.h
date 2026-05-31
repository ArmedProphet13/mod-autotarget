#pragma once

#include <cstdint>
#include <vector>

// Core data types for the AutoTarget engine.
//
// The engine is pure logic: it depends on nothing but the standard library and
// operates only on the plain structs below. The Game Interface layer fills a
// TargetingSnapshot from the live client and applies the resulting
// TargetingDecision. This keeps every offset-bound detail out of the engine and
// makes the engine unit-testable off-client.

namespace autotarget {

using Guid = std::uint64_t;
constexpr Guid kNoGuid = 0;

// One unit visible to the local player, captured by the Game Interface.
struct UnitInfo {
    Guid  guid = kNoGuid;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool  alive = false;
    bool  attackable = false;   // local player is allowed to attack it
    bool  critter = false;
    bool  lineOfSight = false;  // pre-traced by the Game Interface
    float healthPercent = 0.0f; // 0..1, reserved for future scoring
};

// Immutable per-tick input to the engine.
struct TargetingSnapshot {
    float playerX = 0.0f;
    float playerY = 0.0f;
    float playerZ = 0.0f;
    float cameraYaw = 0.0f;      // radians; 0 = +X, counter-clockwise (atan2 convention)
    bool  inCombat = false;
    bool  actionPending = false; // an offensive action was initiated this tick
    Guid  currentTarget = kNoGuid;
    Guid  manualHold = kNoGuid;  // last target the player picked by hand (0 = none)
    Guid  previousSoftTarget = kNoGuid; // last tick's soft pick; engine fills this so the
                                        // scorer can reward stickiness (anti-flicker).
    std::uint32_t nowMs = 0;
    std::vector<UnitInfo> units;
};

// Tuning the engine needs. Populated from ConfigManager by the Orchestration.
struct EngineConfig {
    float tier1Range = 2.5f;            // yards - true point-blank brawl bubble
    float tier1HalfAngleRad = 1.309f;   // unused: tier1 is range-only (no angle gate)
    float tier2Range = 40.0f;           // yards (caster max range)
    float tier2HalfAngleRad = 0.314f;   // ~18 degrees
    float hysteresisBonus = 0.20f;      // score bump for previous soft pick / current target
    bool  aggressiveReaim = true;
    std::uint32_t softTargetGraceMs = 400; // keep last soft target briefly if none found
    std::uint32_t softCommitDwellMs = 200; // a new winner must hold the lead this long
                                            // before it steals the soft pick (anti-twitch)
};

enum class Tier {
    None,
    One,  // brawl zone: wide cone, short range
    Two   // aimed zone: narrow cone, long range
};

// A unit that passed filtering, with its geometry and score.
struct ScoredCandidate {
    Guid  guid = kNoGuid;
    Tier  tier = Tier::None;
    float distance = 0.0f;
    float angleOffset = 0.0f; // radians, absolute
    float score = 0.0f;
};

enum class DecisionKind {
    NoChange,
    SetHardTarget
};

// What sort of thing the soft pick is. Today the engine only ever emits
// Hostile (the candidate scan keeps only attackable units), but every
// downstream consumer - mouseover write, cast handoff, tooltip - carries this
// tag so a future "interaction" module can add Friendly (right-click unit
// menu) and Object (use/interact) picks as new enum values + handlers without
// re-threading the spine. Keep it kind-agnostic: never assume "enemy".
enum class PickKind {
    Hostile,   // attackable unit - the only kind shipped today
    Friendly,  // friendly/neutral player or NPC (future: unit menu)
    Object     // GameObject - chest, lever, herb, quest box (future: interact)
};

// Immutable per-tick output of the engine.
struct TargetingDecision {
    DecisionKind kind = DecisionKind::NoChange;
    Guid hardTarget = kNoGuid;  // meaningful when kind == SetHardTarget
    Guid softTarget = kNoGuid;  // always set: best candidate (diagnostics / commit-on-action)
    PickKind softKind = PickKind::Hostile; // what the soft pick is (always Hostile today)
    const char* reason = "idle";
};

} // namespace autotarget
