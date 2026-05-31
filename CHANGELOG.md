# Changelog

All notable changes to this project are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning follows
[SemVer](https://semver.org/).

---

## v0.3.5 — In-game checkbox, hotkey removed

### Hotkey removed

The toggle hotkey is gone. Not defaulted-off — removed: `HotkeyManager`
deleted, `[input]` INI section deleted, `toggleHotkeyVK` config field
deleted. There is no key that disables AutoTarget mid-fight by accident
because there is no key.

Toggle AutoTarget via:
- the **Enable AutoTarget** checkbox in *Interface Options → Combat*, or
- the `/at on` / `/at off` chat command.

Also accepts a different exe filename. v0.3.4 already let Ascension etc.
through after the launcher-rename complaint. (Image base 0x00400000 is
still required - that's the real safety net.)

### Checkbox actually installs now

The Interface Options checkbox needs a Lua→C bridge via
`FrameScript_RegisterFunction`. Through v0.3.4 the offset for that
function was `0x00000000` (unset), which made `EnableToggle::Install`
return early with the "in-game UI disabled" warning.

v0.3.5 sets `kFnFrameScriptRegisterFunction = 0x00817F90` (single-source
`[VERIFY]`) and wraps the call site in SEH so a wrong offset cannot
crash the client - on first fault the bridge latches off, the warning
is logged once, and the mod keeps running.

If the address is correct, the checkbox lives at the bottom-left of
Interface Options > Combat after the first ~5 seconds in-world. The
chat command `/at on|off` works identically.

### Migration

- Remove any `[input]` section from `AutoTarget.ini` (silently ignored
  if left in - no warnings).
- Toggle the mod via the checkbox or `/at`.

---

## v0.3.4 — SmartUnstick

The 3.3.5a client never auto-clears the active target slot. Out-of-range,
behind a wall, server-leashed (evading), and "wolf attacked from behind
while I had a different mob targeted" all leave the slot held with an
unreachable GUID. The next cast errors out with "Out of range" or "Target
not in line of sight" and AT (correctly, per the v0.3.0 sacred-slot rule)
won't override an apparently-valid active target.

v0.3.4 introduces **`SmartUnstick`** — a labeled, configurable feature
that broadens "slot is unusable" beyond v0.3.3's dead-only case. Each
reason is deterministic, not a guess at player intent.

### Reasons

| Reason | Condition |
|---|---|
| **Dead** | active GUID resolves to a unit with Health == 0 (the v0.3.3 corpse case). |
| **Missing** | active GUID is no longer in the object manager (despawned). |
| **OutOfRange** | active is farther than `SmartUnstickMaxRangeYards` (default 40 yd, the WoW cast cap). |
| **Evading** | active is alive but `IsAttackable()` flipped false (server leash). |
| **BehindSoftCloser** | active is >90° off facing, AND a soft pick exists in the aim cone, AND that soft pick is *closer* than active. |
| **OutOfLoS** | active not visible from player. Currently no-op (`LineOfSight::Visible` is permissive in v1); scaffolded for a future terrain trace. |

Hard rule: **only the active target slot's own GUID is ever evaluated.**
Other corpses, other unreachable mobs, anything else in the world — none
of AT's business.

### Where it fires

- **Controller tick (~70 ms)** — full evaluation, light + heavy reasons.
  On any non-None reason the slot is cleared via `Selection::SetTarget(kNoGuid)`
  bracketed with `SelectionHook::BeginCommit/EndCommit`. Log line:
  `unstick: <Reason> active=<guid>`.
- **Cast hook (synchronous on key press)** — light-only fast path
  (`IsActiveTargetUnusableCheap`) so the player isn't locked out of a
  cast in the ~70 ms gap between ticks. Cast-skip log gains
  `(unusable->empty)` when this fires.

### Dead/Missing are unconditional

Players already rely on v0.3.3's corpse handling. The `SmartUnstick` flag
gates only the *new* reasons (OutOfRange, Evading, BehindSoftCloser,
OutOfLoS). Flipping `SmartUnstick = false` reverts to v0.3.3 behaviour;
Dead/Missing still clear.

### Why BehindSoftCloser is safe for key-spammers

The rule requires the **active target to be >90 degrees behind** the
player's facing. A tank spamming Heroic Strike at the front of a pack
has the active target *in front of them* — the rule cannot fire. It
only triggers when the player has unambiguously turned around.

### Pure-logic seam

The whole decision lives in `Engine/UnstickPolicy.{h,cpp}` — no client
reads, fully unit-testable off-line. New tests cover each reason plus
two negative cases (behind but soft farther; behind but no soft pick).

### Config

```ini
[behaviour]
SmartUnstick = true                  # default on
SmartUnstickMaxRangeYards = 40
```

---

## v0.3.3 — Corpse-clear + soft-pick highlight

Two fixes on top of v0.3.2.

### Active target slot is cleared on corpse

v0.3.2 stopped the cast-lockout: the cast hook treated a corpse in the slot
as empty for the commit decision, so the next spell would acquire on the
new mob. But the slot *itself* still held the dead GUID, so the unit frame
kept showing the kill for ~15 seconds before the client auto-cleared it.

The controller tick now actively clears the slot whenever **its own
contents** are a corpse-or-missing unit (`Selection::SetTarget(kNoGuid)`,
bracketed with `SelectionHook::BeginCommit/EndCommit` so it does not
latch a manual hold). Strictly scoped: we only touch the active target
slot, and only when that slot's GUID is the corpse — other dead bodies
in the world are not our concern.

Log line `corpse-clear: dropping dead active target <guid>` records each
event.

### Soft-pick highlight via `CGGameUI::Set_Mouseover`

Since v0.3.0 the mouseover slot has been written every frame with the
engine's soft pick, but the *visual* yellow ring + tooltip never
appeared. Cause: the raw slot write at `kStaticMouseoverGuid` makes
`[target=mouseover]` macros resolve, but the highlight effect is driven
by the client's `CGGameUI::Set_Mouseover` routine which also flags the
unit's render state and opens the tooltip.

`Selection::SetMouseover` now does both:

1. Raw slot write (always — guarantees macro behaviour is preserved).
2. Call to `CGGameUI::Set_Mouseover` at `kFnSetMouseover` (drives the
   visible highlight), wrapped in structured-exception handling. On
   the first fault the call latches off permanently and only the raw
   write runs — so a wrong offset cannot crash the client, only fall
   back to v0.3.2 behaviour with a warning in the log.

Cursor priority is unchanged. The controller still yields the entire
frame's write when the cursor is asserting a unit, so
`[target=mouseover]` reactive macros (Counterspell-on-add, hover heals)
keep working exactly as before.

`kFnSetMouseover` is `[VERIFY]` — single source. If the address is
wrong on a given client build, the log shows
`Selection: kFnSetMouseover faulted - highlight call disabled` exactly
once and the mod continues with macro-functional, visually-invisible
behaviour.

---

## v0.3.2 — Corpse-in-slot fix

The WoW 3.3.5a client leaves a dead mob's GUID in the active target slot
for ~15 seconds after death before auto-clearing it. v0.3.1's cast hook
treated this as "active target = sacred, respect it" and skipped every
cast until the corpse cleared, leaving the player unable to re-acquire
on aim alone.

### Fix

`AutoTargetController::IsTargetCorpseOrMissing(guid)` looks the GUID up
in the client's object manager and returns true if the unit is dead or
has despawned. The cast hook calls this before deciding to commit: a
corpse counts as an empty slot, so the soft pick is acquired and the
cast resolves on the new mob.

The active target frame still shows the corpse until the client clears
it - that's the client's UI behaviour, unchanged - but it no longer
blocks AutoTarget from doing its job.

### Log

A cast that fires after the corpse-handling now reads
`cast commit: clearing corpse <old-guid>, acquiring <soft-pick>` so the
transition is visible in debug logs.

---

## v0.3.1 — Highlight persistence

Polish patch for v0.3.0. Fixes a subtle visual issue: the mouseover ring
would occasionally blink out during stable aim.

### Cause

The mouseover writer in v0.3.0 only re-wrote the slot when the soft pick
*changed*. The client clears the mouseover slot to 0 on its own when the
player's cursor briefly hovers a unit then leaves — and because our soft
pick had not changed, the writer skipped the next frame's refresh.
Result: the slot stayed at 0 until the engine picked a different mob.

### Fix

The writer now re-asserts the soft pick every frame whenever the cursor
isn't currently competing. Cost: one 64-bit memory store per frame.
Benefit: the highlight ring stays pinned to the aim pick across
cursor-off cycles.

Cursor yield still works identically. `[target=mouseover]` macros remain
cursor-driven.

---

## v0.3.0 — Mouseover preview + Tab swap

v0.2.x acted invisibly: AutoTarget chose a target, you cast, you found out
whether it was right. v0.3.0 gives the player **constant visual feedback**
by writing the soft pick into the mouseover slot, and upgrades the native
Tab keybind into an aim-driven swap.

### Mouseover preview

The engine's soft pick is written to the mouseover slot every frame, so
the player sees a **yellow mouseover ring + tooltip** on whichever enemy
AutoTarget is currently aiming at. No new UI to render, no overlay to
draw — we reuse the client's own highlight.

### Cursor always wins

The mouseover writer **yields to the cursor**: when the client writes a
unit GUID into the mouseover slot from a cursor hover, we observe it and
stop writing this frame. `[target=mouseover]` macros (reactive
Counterspell, heals on hovered party members) work exactly as Blizzard
designed — AutoTarget never fights the cursor.

### Tab hijack

Pressing Tab now writes the soft pick into the active target slot —
aim-driven swap, replacing the native cycle-by-proximity behaviour that
often picks the wrong mob in a tight pack. If you press Tab while
aiming at empty space, the hook falls through to native cycle so Tab
never does *less* than before.

### Removed: in-combat tick-promote

v0.2.2 silently wrote the soft pick into the active target slot whenever
the slot went empty in combat. v0.3.0 removes this — the always-visible
mouseover ring already shows the next victim the instant the previous
target dies, and the player commits explicitly via cast or Tab.
AutoTarget never silently changes the active target slot.

### Behaviour summary

| Slot | Owner | When AutoTarget writes |
|---|---|---|
| Active target | sacred — only player explicit input | cast hook on empty slot; Tab hijack on press |
| Mouseover | shared — cursor wins, AT fills silence | every frame when cursor isn't asserting |

### Known limitation (still v0.4)

Heal in open space with no friendly cursor-hovered AND a hostile mob in
the aim cone will still acquire the mob via the cast hook. Real fix is a
harmful-spell flag read from the spell record.

---

## v0.2.2 — Coexist with native targeting

v0.2.1 over-corrected: every cast re-aimed, including stomping the
player's tab/clicked target. v0.2.2 restores the retail Blizzard Action
Targeting model: soft targeting fills in **only** when the native target
slot is empty.

### Behaviour

| Situation | Target slot | In combat | What AutoTarget does |
|---|---|---|---|
| Idle in town | empty | no | nothing (soft stays invisible) |
| First pull | empty | no | cast hook acquires on the first offensive cast |
| Active target alive | non-empty | yes | leaves the slot alone |
| Active target dies | empty | yes | tick-driven promote: soft pick becomes target within ~70 ms, no key press needed |
| Player tab / clicks a mob | non-empty | yes | leaves the slot alone (tab wins) |
| Player ESCs out | empty | yes/no | re-acquires in combat; waits for a cast out of combat |

### Fixes

- Restored the "respect non-empty target slot" rule that v0.2.1 deleted.
  Tab, click, and our own previous commits are all respected equally:
  if the target slot has a value, aim does not override it.
- Added in-combat tick-driven promote: when the active target dies (or
  is ESC'd out) the soft pick becomes the new target immediately, with
  no need for a cast to trigger the swap. Out of combat the soft pick
  stays invisible to avoid grabbing critters while running through town.

### Mouseover slot is sacred

ActionTarget mode never writes the mouseover slot. Cursor-driven
`[target=mouseover]` macros (e.g. reactive `Counterspell`) continue to
work exactly as Blizzard intended; AutoTarget does not race the cursor.

### Known limitation (still banked for v0.3)

Heal in open space with no friendly selected AND a hostile mob in the
aim cone still acquires the mob (server rejects, easy retarget). Real
fix is a harmful-spell flag read from the spell record.

---

## v0.2.1 — Aim wins every cast

Hot follow-up to v0.2.0 after first live-fire testing. The v0.2.0 commit
policy had three gates that each broke the design intent in the field;
all three are removed.

### Fixes

- **Removed the offensive-spell whitelist.** `Spell_C_CastSpell` is called
  with rank-specific IDs (Fireball alone has 16 ranks), so a usable
  whitelist would need to enumerate every rank of every offensive spell
  across every class. The mouseover mechanism never filtered by spell and
  worked fine; ActionTarget now follows the same model. No `[offensive]
  SpellIds` config; any stale section in your ini is silently ignored.
- **Removed the "respect existing target" rule.** It made every cast
  *after* the first one freeze onto the previous commit's mob, the exact
  opposite of Blizzard Action Targeting. The new rule: every offensive
  cast re-aims onto whatever you're pointing at.
- **Removed the manual-hold gate.** Once SelectionHook flagged a tab/click
  selection it stayed flagged until the held unit died, locking casts out
  indefinitely. Pressing the cast button is itself a re-aim command;
  there is no separate "manual hold wins" mode any more.

### Commit decision now

```
return softGuid != kNoGuid;
```

That's the whole rule. If you're aiming at an enemy, the next cast goes
there. If you're not, the client raises its normal no-target error.

### Known limitation (banked for v0.3)

A single-target heal pressed in open space with no friendly selection AND
a hostile mob in the aim cone will set the hostile mob as your target;
the server then rejects the cast with "you cannot help that target". Real
fix needs a `Spell.dbc` / spell-record harmful-flag read (v0.3 scope).

---

## v0.2.0 — Action Targeting + version.dll proxy

Major usability release. Casts now follow your aim without macros, and the
DLL ships under a name that almost never collides with other mods.

### Features

- **Real Action Targeting via `SpellCastHook`.** A new `Mechanism = action`
  mode detours the client's spell-cast entry point. When you press an
  offensive ability with no valid hostile target, the engine's soft pick is
  injected as your hard target *before* the client's no-target check.
  Plain `/cast Fireball` works — no mouseover macros required.
  - Opt-in offensive-spell whitelist (`[offensive] SpellIds = ...` in
    `AutoTarget.ini`), default-populated for mage. Heals / buffs / utility
    casts pass through untouched so support classes are safe.
  - Manual selections (tab, click) still win — the hook backs off whenever a
    deliberate target is held.
  - Commit-decision logic is a pure `ShouldCommitSpellTarget` function in
    the engine layer, covered by four new off-client unit tests.
- **Proxy renamed from `d3d9.dll` to `version.dll`.** WoW.exe also loads
  `version.dll` at process start, but unlike `d3d9.dll`, version.dll is
  almost never replaced by DXVK / HD packs / ReShade / ENB — so the
  chained-DLL install dance becomes a rarely-used fallback rather than the
  single most common mistake.
  - All 16 standard `version.dll` exports forwarded; chain order is
    `version_chain.dll` → `version.dll.disabled` → system `version.dll`.

### Migration from v0.1.0

- Remove the old `d3d9.dll` from your WoW folder. If you renamed an
  original `d3d9.dll` aside as `d3d9_chain.dll`, you can restore it.
- Drop the new `version.dll` and updated `AutoTarget.ini` into the WoW
  folder. The default `Mechanism = action` is the new behaviour; if you
  prefer the v0.1 macro workflow set it back to `Mechanism = mouseover`.

### Known limitations

- `kFnSpellCastSpell` is `[VERIFY]` — single-sourced. If MinHook fails to
  install on your client, ActionTarget silently falls back to mouseover
  behaviour and the log warns. Verify with `Mode = diagnostic` first.
- Spell whitelist is class-specific; non-mage users must populate
  `[offensive] SpellIds` themselves (instructions in the ini).
- Automatic "is this spell hostile?" derivation (Spell.dbc lookup) is
  v0.3 scope.

---

## v0.1.0 — first public release

Initial public release. Implements the full aim → mouseover → cast loop and
ships with the safety, configuration, and documentation needed for end users.

### Features

- **Aim-driven mouseover targeting.** Every frame the client's mouseover slot
  is pinned to the closest attackable enemy in a ±15° cone (40 yd range) in
  front of the player's character, plus a 2.5 yd all-angle "brawl bubble" for
  point-blank fighting.
- **Mouseover or hard-target mechanism.** Configurable: drive the mouseover
  slot (default; spells follow with `/cast [target=mouseover] ...` macros) or
  commit the soft pick directly to the real target slot.
- **Three on/off paths**, all converging on one master enable:
  - Toggle hotkey (default F10, configurable).
  - In-game checkbox in *Interface Options → Combat* (when the native bridge
    offset is set).
  - `/at on` / `/at off` / `/at` chat command.
- **Three run modes** for safe first launch: `off` (graphics passthrough only),
  `diagnostic` (engine reads & logs, never writes), `live` (full operation).
- **Pure-logic engine** with 24 off-client unit tests covering tier
  classification, scoring, brawl-bubble behaviour, soft-target grace, manual
  hold, hysteresis, critter / LoS / behind-player exclusion.
- **Proxy chaining.** Forwards every d3d9 export to a `d3d9_chain.dll`, so the
  mod stacks with existing DXVK wrappers, HD packs, ReShade, ENB, etc.
- **Module pinning** prevents Windows from unloading the DLL while detours are
  still live in the client image.
- **SafeMode** wraps every per-tick body and hook callback in a structured
  exception guard. A fault disables AutoTarget and logs it; the game keeps
  running.

### Known limitations

- The in-game checkbox / `/at` command requires
  `kFnFrameScriptRegisterFunction` to be set in `Offsets.h`. Until that
  address is verified, the UI is skipped at install time (no decorative-only
  control) and the hotkey is the working toggle.
- Camera-yaw read is unverified and falls back to the player's model facing.
  In mouselook this is identical for practical purposes.
- Engine `previousSoftTarget` field is wired but not consulted by the scorer
  — the mage workflow (one cast every ~2 s) needs accuracy at press time, not
  inter-tick stickiness.
