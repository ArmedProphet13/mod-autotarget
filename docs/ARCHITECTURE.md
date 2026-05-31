# AutoTarget — Architecture (original design document)

> This is the original design document drafted before implementation. It is
> kept here for design provenance and to document the *why* behind the
> structural choices. For the **current** architecture summary (what the code
> actually does today) see the *Architecture* section of
> [`client/README.md`](../client/README.md). Some specifics here have evolved
> in the codebase — the layering, separation, and decisions about server vs.
> client implementation are still accurate.

## Context
Modern WoW (Dragonflight, and now Classic/TBC Anniversary) ships Action Targeting:
your aim automatically acquires and maintains a combat target, so melee players in
multi-mob situations never lose DPS to a dead target, a stray tab, or a misclick.

The 3.3.5a client AzerothCore targets has no such system and no way to add one
server-side. Investigation of the AzerothCore core confirmed the hard limit:

UNIT_FIELD_TARGET is UF_FLAG_PUBLIC — a broadcast display field only.
Player::SetSelection() (Player.cpp:11498)
just writes that field; the client owns its real selection.
The only server→client targeting opcode is SMSG_BREAK_TARGET — it can clear
a target, never set one.
Lua addons cannot set a combat target either (protected functions).
This is exactly why Blizzard built Action Targeting inside the client. We have
no native option and no server-side or addon path — so AutoTarget is implemented
as a client-side proxy DLL loaded into WoW.exe (build 12340). It runs as
native code inside the process, so it can call the client's own targeting function
directly with no restrictions.

Goal: a ready-to-ship, open-source DLL that reproduces Blizzard's Action
Targeting pipeline on 3.3.5a — strong, layered, clean architecture.

Decisions locked during design
Proxy DLL, not an injector — loaded by the normal Windows DLL search order
when WoW.exe starts. No CreateRemoteThread, the single biggest AV trigger.
Open-source, no obfuscation → same trust profile as ReShade / addon loaders.
v1 is standalone — DLL + local AutoTarget.ini. No server dependency, works
on any AzerothCore realm. Server companion module = future scope.
One repo — restructure mod-autotarget into client/ (the DLL) and
server-module/ (future AC module).
Client-side project, independent of AzerothCore — this DLL targets the WoW
client only. AzerothCore conventions and its CLAUDE.md (commit format, SQL
layout, -Werror, module loader naming, etc.) govern server code and do not
apply here. The client/ tree has its own toolchain, style, and build.
Targeting model (from design phase):
Aim off the camera yaw, not model facing — what the player is looking at.
Soft target: always-on, silent, free. Tracks the best cone candidate every
tick. Never touches the hard target by itself.
Commit-on-action: the soft target becomes the real hard target only when the
player acts (offensive ability press / auto-attack / already in combat).
Aggressive re-aim ON by default: in combat the hard target follows the
camera, with hysteresis to prevent flicker.
Manual override wins: a deliberate click/tab pauses re-aim on that target
until it dies or is cleared.
No weapon-sheath gate — the always-on soft target is invisible, so walking
through town never grabs critters; only the commit step is visible.
Scope: enemy targeting only (softenemy). Friendly/interact = future.
Player-facing on/off toggle — a single "Enable AutoTarget" checkbox in the
in-game Interface Options → Combat panel, Blizzard-faithful (Blizzard exposes
Action Targeting there too). See Enable Toggle & Input below.
Targeting Pipeline (mirrors Blizzard)
Blizzard: soft target tracked continuously → highlighted → used on action when no
hard target → hard target has priority. Ours is the same pipeline squeezed into the
single available target slot (no highlight, by choice).

Per tick (~60–80 ms, on the game thread):

Snapshot — read local player position, camera yaw, combat state, current
hard target, and all units in range (GUID, position, reaction, health, creature
type, flags).
Scan — filter candidates: alive, attackable, hostile, not a critter, in
range, inside the cone, in line of sight.
Classify (tiers)
Tier 1 — brawl zone: wide cone (~140–160°), short range (~8 yd). Pick the
closest. Anything on top of you is fair game.
Tier 2 — aimed zone: narrow cone (~30–40°), long range (~30 yd). Pick the
most-centered, distance as tiebreaker. You must point at it.
Tier 1 always beats Tier 2.
Score — rank within tiers; apply a hysteresis bonus to the current target
so a challenger must be meaningfully better to displace it.
Soft target — update the always-on soft target = best candidate.
Decide (state machine + commit policy)
State A — no valid hard target + (in combat or action pending) → commit soft.
State B — valid target + in combat + aggressive mode + no manual hold →
re-aim hard target toward soft.
State B — manual hold → do nothing.
Out of combat, idle → soft tracking only, no hard write.
Apply — if committing, call the client's internal SetTarget.
Event-driven kicks (bypass the throttle):

Current target dies → immediate re-evaluation (no gap before next GCD).
Offensive ability pressed with no valid hard target → commit soft target
synchronously, before the client's own no-target check → cast proceeds.
Manual selection detected → set the manual-hold flag.
Layered Architecture
Strict dependency direction: Engine depends on nothing. Everything offset-bound
is quarantined in Game Interface. The Engine is pure logic and unit-testable
off-client.

Bootstrap  ──>  Orchestration  ──>  Engine (pure, no client deps)
                      │
                      └────────>  Game Interface (all offsets live here)
Config, Input, Diagnostics are cross-cutting (used by Orchestration).
1. Bootstrap
DllMain, proxy-export forwarding for the proxied DLL.
On attach: verify client build/signatures, load config, install hooks, start the
controller. On detach: uninstall hooks cleanly.
If signature verification fails (non-12340 client) → self-disable, log, never
touch the client.
2. Game Interface — the only layer that knows offsets
Offsets.h — single source of truth for all 12340 addresses/signatures.
ObjectManager — enumerate the client object list.
WoWObject / WoWUnit / WoWPlayer — typed accessors (position, GUID, health,
flags, reaction, creature type, target GUID).
Camera — camera position + yaw.
Selection — call the client's internal SetTarget; read current target.
LineOfSight — terrain/collision trace via the client's own routine.
Hooks: FrameHook (D3D9 EndScene vtable hook — offset-free, runs on the game
thread), SpellCastHook (commit-on-action), SelectionHook (detect manual picks).
3. Engine — pure logic, no client dependency, testable
TargetingSnapshot — immutable input struct (player state + unit list).
CandidateScanner — filtering.
ConeModel — tier classification + geometry.
TargetScorer — scoring + hysteresis.
SoftTargetTracker — maintains the always-on soft target.
TargetingStateMachine — State A/B, manual-hold tracking.
CommitPolicy — when to write the hard target.
TargetingDecision — immutable output struct.
4. Orchestration
AutoTargetController — per-tick coordinator: throttle, build snapshot via Game
Interface, run Engine, apply TargetingDecision.
EventDispatcher — routes target-death / enter-combat / ability-press /
manual-selection events to the controller.
5. Config
AutoTarget.ini next to the DLL — tuning only: tick rate, Tier 1/2 cone
widths and ranges, hysteresis weight, aggressive-mode toggle, toggle keybind,
log level. (The on/off enable state is a CVar, not the ini — see below.)
ConfigManager — parse, defaults, reload.
6. Enable Toggle & Input
The on/off state is separate from the tuning config so players can flip it
in-game and have it persist.

Single source of truth: a persisted CVar (autoTargetEnabled). CVars are
saved by the client automatically and are readable from both Lua and native code.
The controller reads it as the master enable each tick (cheap).
In-game UI checkbox — at load, the DLL executes a small embedded Lua snippet
that adds one line, "Enable AutoTarget", to the Interface Options → Combat
panel (InterfaceOptionsCombatPanel), bound to the CVar. Standard FrameXML; no
separate addon to distribute — the DLL injects the Lua itself. (Fallback if
in-DLL Lua injection proves fiddly: ship a tiny one-checkbox companion addon.)
HotkeyManager — optional keybind that flips the same CVar.
ChatCommands — /at on|off|reload|status via a chat-input hook; also flips
the CVar.
All three paths converge on the one CVar, so UI, keybind, and command stay in sync.
7. Diagnostics
Logger — leveled file log (no debugger attach in production).
SafeMode — every tick + hook wrapped in SEH/try-catch; a fault disables
AutoTarget and logs, never crashes WoW.exe. Optional debug mode dumps the
scored candidate list each tick for tuning.
Repository Layout
Restructure the existing mod-autotarget repo:

mod-autotarget/
  client/                         # the DLL — primary deliverable
    src/
      Bootstrap/      DllMain, ProxyExports, Initializer
      GameInterface/  Offsets.h, ObjectManager, WoWObject/Unit/Player,
                      Camera, Selection, LineOfSight, FrameHook,
                      SpellCastHook, SelectionHook
      Engine/         TargetingSnapshot, CandidateScanner, ConeModel,
                      TargetScorer, SoftTargetTracker, TargetingStateMachine,
                      CommitPolicy, TargetingDecision
      Orchestration/  AutoTargetController, EventDispatcher
      Config/         ConfigManager
      Input/          HotkeyManager, ChatCommands
      Diagnostics/    Logger, SafeMode
    vendor/           MinHook (MIT — function detours)
    tests/            Engine unit tests (off-client harness)
    CMakeLists.txt    # 32-bit MSVC, targets the WoW process
    AutoTarget.ini    # default config shipped with the DLL
    README.md
  server-module/                  # future scope — the AC companion module
    (current scaffolded mod-autotarget files move here)
  README.md
The current skeleton files (src/AutoTarget.cpp, src/AutoTarget_loader.cpp,
conf/, data/, CMakeLists.txt) move under server-module/ untouched.

Hooking Strategy
MinHook (MIT, tiny, unobfuscated, AV-friendly) for all detours.
Frame tick: hook D3D9 EndScene via its vtable — offset-independent, and on
the 3.3.5 single-threaded client it runs on the game thread, which is mandatory
(the client is not reentrant).
Commit-on-action: detour the client's cast-spell function; our handler runs
before its no-target check.
Manual detection: detour the client's SetTarget so a player-initiated
selection is distinguishable from our own commit.
All hooks use proper trampolines and are removed cleanly on detach.
Safety & Robustness
Startup signature check; mismatch → graceful self-disable.
SEH/try-catch around every tick and hook callback.
No heap allocation in the hot path where avoidable; reuse buffers.
Master kill-switch (config + hotkey) disables all hard-target writes instantly.
Build & Distribution
32-bit DLL, MSVC, CMake.
Ship: proxy DLL + AutoTarget.ini + README.md. Drop into the WoW folder.
Open-source on the mod-autotarget GitHub repo; no obfuscation.
Deployment note for operators: whitelist the DLL in the realm's Warden config.
Verification
Engine unit tests (client/tests/) — feed hand-built TargetingSnapshots,
assert TargetingDecisions: tier precedence, hysteresis stability, State A/B
transitions, manual-hold. Runs off-client in CI.
In-client scenarios (manual, with debug candidate-dump enabled):
Single-mob pull while questing → instant acquire on first ability press.
Dungeon pack → target dies → next mob acquired before the next GCD.
Aggressive re-aim → camera swing across a pack switches target, no flicker.
Manual click/tab → re-aim pauses until that target dies.
Walk through a town → no targets grabbed (soft tracking stays silent).
LOS-blocked mob → not acquired.
Toggle → "Enable AutoTarget" checkbox appears in Interface Options →
Combat; unchecking it halts all hard-target writes immediately; the state
persists across relog; keybind and /at command stay in sync with it.
Robustness — launch against a deliberately wrong build → DLL self-disables,
logs, client runs normally.
Out of Scope (future)
Friendly targeting (softfriend) and interact targeting (softinteract).
Server companion module (server-module/) — per-realm enable + config push via
addon messages.
Visual highlight of the soft target.