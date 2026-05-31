// Off-client unit tests for the AutoTarget engine.
//
// The engine is pure logic: it links against nothing but the standard library,
// so this harness runs anywhere (and in CI) with no WoW client. Each test
// hand-builds a TargetingSnapshot and asserts the resulting TargetingDecision.
//
// Geometry convention: the player sits at the origin facing +X (cameraYaw 0),
// so a unit at (+d, 0) is dead ahead and a unit at (+x, +y) is offset by
// atan2(y, x) radians.

#include <cstdio>

#include "Engine/EngineTypes.h"
#include "Engine/SpellCommitPolicy.h"
#include "Engine/TargetingEngine.h"
#include "Engine/UnstickPolicy.h"

using namespace autotarget;

// Shared counters — non-anonymous so other test TUs (MechanismHandlerTests,
// ToggleManagerTests) can extern them and contribute to one summary.
int g_pass = 0;
int g_fail = 0;

// Forward decls for the runners defined in sibling test TUs.
void RunMechanismHandlerTests();
void RunToggleManagerTests();

namespace {

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                      \
    } while (0)

// A live, attackable, visible enemy at horizontal (x, y).
UnitInfo Enemy(Guid guid, float x, float y) {
    UnitInfo u;
    u.guid          = guid;
    u.x             = x;
    u.y             = y;
    u.z             = 0.0f;
    u.alive         = true;
    u.attackable    = true;
    u.critter       = false;
    u.lineOfSight   = true;
    u.healthPercent = 1.0f;
    return u;
}

TargetingSnapshot Snapshot() {
    TargetingSnapshot s;
    s.playerX   = 0.0f;
    s.playerY   = 0.0f;
    s.playerZ   = 0.0f;
    s.cameraYaw = 0.0f; // facing +X
    s.nowMs     = 1000;
    return s;
}

// --- Tests ------------------------------------------------------------------

// In combat with no current target, an enemy in the cone is committed.
void Test_VoidFillInCombat() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    s.units.push_back(Enemy(10, 5.0f, 0.0f)); // Tier 1: close, dead ahead

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 10);
}

// Out of combat and idle, the same enemy is tracked softly but never committed.
void Test_IdleSoftOnly() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = false;
    s.units.push_back(Enemy(10, 5.0f, 0.0f));

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::NoChange);
    CHECK(d.softTarget == 10);
}

// Aim-first scoring: the most-centred enemy wins, even if a closer one is
// off to one side. (Replaces the old "Tier 1 always beats Tier 2" rule -
// closest-wins overrode aim in a pack and was the wrong behaviour.)
void Test_CentredBeatsOffCentre() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    s.units.push_back(Enemy(20, 6.0f, 3.0f));  // close, but off to one side
    s.units.push_back(Enemy(21, 20.0f, 0.0f)); // farther, dead centre

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 21);
}

// A hand-picked target pauses aggressive re-aim even when a better unit exists.
void Test_ManualHoldBlocksReaim() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat      = true;
    s.currentTarget = 30;
    s.manualHold    = 30;
    s.units.push_back(Enemy(30, 12.0f, 12.0f)); // current, off-axis
    s.units.push_back(Enemy(31, 15.0f, 0.0f));  // centred rival

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::NoChange);
}

// With no manual hold, aggressive re-aim switches to the centred unit when
// you turn away from the current one.
void Test_AggressiveReaim() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat      = true;
    s.currentTarget = 30;
    s.manualHold    = kNoGuid;
    s.units.push_back(Enemy(30, 12.0f, 12.0f)); // current, now off-axis
    s.units.push_back(Enemy(31, 15.0f, 0.0f));  // centred - what you point at

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 31);
}

// The HARD target (the one the player is actually attacking) keeps its
// hysteresis bonus - so in HardTarget mode a marginally-better rival can't
// displace what the player is committed to mid-fight. The soft pick has NO
// such stickiness on purpose (see SoftTargetTracker for why).
void Test_HardTargetHysteresisKeepsCurrent() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat      = true;
    s.currentTarget = 40;
    s.units.push_back(Enemy(40, 15.0f, 0.5f)); // current hard: slightly off-centre
    s.units.push_back(Enemy(41, 15.0f, 0.0f)); // rival: a touch more centred

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.softTarget == 40);
}

// A clearly-better rival still wins despite the hysteresis bonus.
void Test_HysteresisYieldsToClearWinner() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat      = true;
    s.currentTarget = 50;
    s.units.push_back(Enemy(50, 25.0f, 3.0f)); // current: far and off-centre
    s.units.push_back(Enemy(51, 15.0f, 0.0f)); // rival: closer and centred

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 51);
}

// The soft target survives a brief candidate dropout, then lapses.
void Test_SoftTargetGrace() {
    TargetingEngine engine(EngineConfig{}); // softTargetGraceMs = 400

    TargetingSnapshot t1 = Snapshot();
    t1.nowMs = 1000;
    t1.units.push_back(Enemy(60, 5.0f, 0.0f));
    CHECK(engine.Evaluate(t1).softTarget == 60);

    TargetingSnapshot t2 = Snapshot(); // no units
    t2.nowMs = 1200;                   // within the 400 ms grace window
    CHECK(engine.Evaluate(t2).softTarget == 60);

    TargetingSnapshot t3 = Snapshot(); // no units
    t3.nowMs = 1500;                   // 500 ms since last seen -> lapsed
    CHECK(engine.Evaluate(t3).softTarget == kNoGuid);
}

// Critters are never acquired, even sitting on top of the player in combat.
void Test_CritterExcluded() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    UnitInfo critter = Enemy(70, 2.0f, 0.0f);
    critter.critter = true;
    s.units.push_back(critter);

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::NoChange);
    CHECK(d.softTarget == kNoGuid);
}

// A unit with no line of sight is filtered out.
void Test_LineOfSightExcluded() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    UnitInfo blocked = Enemy(80, 5.0f, 0.0f);
    blocked.lineOfSight = false;
    s.units.push_back(blocked);

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::NoChange);
}

// A distant unit behind the player (beyond the brawl range, outside the aimed
// cone) is not acquired. Close units behind the player ARE acquired by design:
// Tier 1 is range-only because point-blank angles are noise.
void Test_BehindPlayerExcluded() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    s.units.push_back(Enemy(90, -25.0f, 0.0f)); // far and directly behind

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::NoChange);
    CHECK(d.softTarget == kNoGuid);
}

// Inside the brawl bubble (<= tier1Range) the closest unit wins regardless of
// angle. Two units both within the bubble; the nearer one is picked even if the
// other is more directly centred on the aim.
void Test_BrawlBubbleClosestWins() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    s.units.push_back(Enemy(100, 0.9f, 1.2f)); // 1.5 yd, off to the side - CLOSER
    s.units.push_back(Enemy(101, 2.4f, 0.0f)); // 2.4 yd, dead centre but farther

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 100); // closer wins inside the brawl bubble (angle ignored)
}

// Outside the brawl bubble, a flanking mob never beats the aimed one - even if
// it is much closer. Accuracy alone decides outside point-blank range.
void Test_NoFlankPickOutsideBrawl() {
    TargetingEngine engine(EngineConfig{});
    TargetingSnapshot s = Snapshot();
    s.inCombat = true;
    s.units.push_back(Enemy(110, 5.0f, 8.7f)); // 10 yd at ~60deg - out of cone
    s.units.push_back(Enemy(111, 20.0f, 0.0f)); // far but dead-centre, aimed

    const TargetingDecision d = engine.Evaluate(s);
    CHECK(d.kind == DecisionKind::SetHardTarget);
    CHECK(d.hardTarget == 111);
}

} // namespace

// --- SpellCommitPolicy tests ------------------------------------------------
// v0.2.1: the decision is "do we have a soft pick?". The previous gates
// (offensive-spell whitelist, respect-existing-target, manual-hold) were
// removed because they each broke the design intent ("aim wins, every cast")
// in the field. The hook's caller still passes spellId and incomingTargetGuid
// purely for diagnostic logging.

void Test_CommitPolicy_EmptySlotCommits() {
    std::printf("Test_CommitPolicy_EmptySlotCommits\n");
    // Target slot empty (0) + soft pick exists -> commit.
    CHECK(ShouldCommitSpellTarget(133, kNoGuid, 0xABCD) == true);
    // Both no-target sentinels also count as empty.
    CHECK(ShouldCommitSpellTarget(133, 0xFFFFFFFFFFFFFFFEull, 0xABCD) == true);
    CHECK(ShouldCommitSpellTarget(133, 0xFFFFFFFFFFFFFFFFull, 0xABCD) == true);
}

void Test_CommitPolicy_RespectsActiveTarget() {
    std::printf("Test_CommitPolicy_RespectsActiveTarget\n");
    // Active target set (whether by tab/click or a previous commit) -> skip.
    // Matches retail Blizzard Action Targeting: aim never overrides an
    // already-acquired target.
    CHECK(ShouldCommitSpellTarget(133, 0x1111, 0xABCD) == false);
}

void Test_CommitPolicy_NoSoftPick() {
    std::printf("Test_CommitPolicy_NoSoftPick\n");
    CHECK(ShouldCommitSpellTarget(133, kNoGuid, kNoGuid) == false);
    CHECK(ShouldCommitSpellTarget(133, 0x1111, kNoGuid) == false);
}

// --- SmartUnstick (UnstickPolicy) tests -------------------------------------
// Pure-logic predicate, no client reads. Each test builds the input bundle by
// hand. Convention: player at origin facing +X. "Active in front" -> activeDx > 0.

UnstickInputs ValidActiveInputs() {
    UnstickInputs in;
    in.activeExists     = true;
    in.activeAlive      = true;
    in.activeAttackable = true;
    in.activeDx         = 10.0f;  // 10 yd in front
    in.activeDy         = 0.0f;
    in.activeDz         = 0.0f;
    in.activeVisibleLoS = true;
    in.playerFacingRad  = 0.0f;
    in.softPick         = kNoGuid;
    in.maxRangeYards    = 40.0f;
    return in;
}

void Test_Unstick_NoneWhenValid() {
    std::printf("Test_Unstick_NoneWhenValid\n");
    UnstickInputs in = ValidActiveInputs();
    CHECK(EvaluateUnstickReason(in)      == UnstickReason::None);
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::None);
}

void Test_Unstick_DeadActive() {
    std::printf("Test_Unstick_DeadActive\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeAlive = false;
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::Dead);
    CHECK(EvaluateUnstickReason(in)      == UnstickReason::Dead);
}

void Test_Unstick_MissingActive() {
    std::printf("Test_Unstick_MissingActive\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeExists = false;
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::Missing);
    CHECK(EvaluateUnstickReason(in)      == UnstickReason::Missing);
}

void Test_Unstick_OutOfRange() {
    std::printf("Test_Unstick_OutOfRange\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeDx = 41.0f; // 41 yd > 40 cap
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::OutOfRange);
    in.activeDx = 39.0f;
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::None);
}

void Test_Unstick_Evading() {
    std::printf("Test_Unstick_Evading\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeAttackable = false; // alive but server flipped it
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::Evading);
}

void Test_Unstick_BehindSoftCloser() {
    std::printf("Test_Unstick_BehindSoftCloser\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeDx = -15.0f; // 15 yd directly behind
    in.softPick = 0xABCD;
    in.softDx   = 8.0f;   // 8 yd in front - closer than 15
    in.softDy   = 0.0f;
    in.softInCone = true;
    CHECK(EvaluateUnstickReason(in)      == UnstickReason::BehindSoftCloser);
    // Light-only must not see this heavy reason.
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::None);
}

void Test_Unstick_BehindButFarSoft() {
    std::printf("Test_Unstick_BehindButFarSoft\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeDx = -10.0f;   // 10 yd behind
    in.softPick = 0xABCD;
    in.softDx   = 25.0f;    // 25 yd in front - FARTHER than active
    in.softInCone = true;
    CHECK(EvaluateUnstickReason(in) == UnstickReason::None);
}

void Test_Unstick_BehindNoSoft() {
    std::printf("Test_Unstick_BehindNoSoft\n");
    UnstickInputs in = ValidActiveInputs();
    in.activeDx = -15.0f;
    in.softPick = kNoGuid; // no soft pick to swap to
    CHECK(EvaluateUnstickReason(in) == UnstickReason::None);
}

void Test_Unstick_LightSkipsHeavy() {
    std::printf("Test_Unstick_LightSkipsHeavy\n");
    UnstickInputs in = ValidActiveInputs();
    // Heavy conditions hold (active behind, soft in front + closer)...
    in.activeDx = -15.0f;
    in.softPick = 0xABCD;
    in.softDx   = 5.0f;
    in.softInCone = true;
    // ...but Light alone must not report BehindSoftCloser.
    CHECK(EvaluateUnstickReasonLight(in) == UnstickReason::None);
    CHECK(EvaluateUnstickReason(in)      == UnstickReason::BehindSoftCloser);
}

void Test_TargetSlotEmpty() {
    std::printf("Test_TargetSlotEmpty\n");
    CHECK(IsTargetSlotEmpty(kNoGuid) == true);
    CHECK(IsTargetSlotEmpty(0xFFFFFFFFFFFFFFFEull) == true);
    CHECK(IsTargetSlotEmpty(0xFFFFFFFFFFFFFFFFull) == true);
    CHECK(IsTargetSlotEmpty(0x1111) == false);
    CHECK(IsTargetSlotEmpty(0xF130000000000001ull) == false);
}

int main() {
    std::printf("AutoTarget engine tests\n");

    Test_VoidFillInCombat();
    Test_IdleSoftOnly();
    Test_CentredBeatsOffCentre();
    Test_ManualHoldBlocksReaim();
    Test_AggressiveReaim();
    Test_HardTargetHysteresisKeepsCurrent();
    Test_HysteresisYieldsToClearWinner();
    Test_SoftTargetGrace();
    Test_CritterExcluded();
    Test_LineOfSightExcluded();
    Test_BehindPlayerExcluded();
    Test_BrawlBubbleClosestWins();
    Test_NoFlankPickOutsideBrawl();

    Test_CommitPolicy_EmptySlotCommits();
    Test_CommitPolicy_RespectsActiveTarget();
    Test_CommitPolicy_NoSoftPick();
    Test_TargetSlotEmpty();

    Test_Unstick_NoneWhenValid();
    Test_Unstick_DeadActive();
    Test_Unstick_MissingActive();
    Test_Unstick_OutOfRange();
    Test_Unstick_Evading();
    Test_Unstick_BehindSoftCloser();
    Test_Unstick_BehindButFarSoft();
    Test_Unstick_BehindNoSoft();
    Test_Unstick_LightSkipsHeavy();

    RunToggleManagerTests();
    RunMechanismHandlerTests();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
