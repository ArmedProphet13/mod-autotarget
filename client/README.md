# AutoTarget

A client-side aim-driven targeting system for the WoW 3.3.5a (WotLK,
build 12340) client used by AzerothCore-based realms. Inspired by Blizzard's
modern Action Targeting.

AutoTarget watches the direction your character is facing and keeps a soft
pick on whichever attackable enemy is closest to the line in front of you.
When you cast an offensive spell with no target, that pick is injected as your
real target before the client's no-target check — `/cast Fireball` works as
"Action Targeting", no macros required. A legacy mouseover mode is still
shipped as a fallback.

It ships as a **proxy DLL** loaded by the normal Windows DLL search order. It
performs no remote thread injection, contains no obfuscation, and is fully open
source — the same trust profile as ReShade or an addon loader.

---

## How it works

Every frame, on the game thread, AutoTarget:

1. Reads your character's position, facing, and the list of nearby units.
2. Filters out anything that isn't a live, attackable, non-critter enemy in line
   of sight.
3. Picks a winner:
   - **Brawl bubble** (≤ 2.5 yd): closest enemy wins regardless of angle. Aim
     is meaningless at point-blank — `atan2` on a sub-yard vector is noise.
   - **Aimed cone** (≤ 40 yd, ±15° around your facing): most-centred enemy
     wins. Distance is only a tiebreaker.
4. Remembers the winner as the *soft pick*. No real target is touched yet.

When you actually press an offensive ability (in **ActionTarget** mode, the
default), AutoTarget detours the client's spell-cast function and — if you
have no real target, no manual hold, and the spell is on the offensive
whitelist — sets the soft pick as your real target *before* the client's
no-target check. The cast resolves, the mob lights up. Plain `/cast Fireball`
works.

If you prefer to drive the **mouseover** slot instead (so your real target is
never touched), flip `Mechanism = mouseover` in the ini and use `/cast
[target=mouseover] ...` macros (3.3.5a syntax — note the `target=` prefix,
**not** the modern `@mouseover`).

---

## Requirements

- WoW 3.3.5a, **build 12340**. Other builds are not supported and the DLL will
  self-disable if it detects a mismatch.
- Windows. The DLL is 32-bit (matching the client).
- To build from source: Visual Studio 2019 or 2022 with the C++ workload,
  CMake 3.20+.

---

## Install (binary release)

1. Download the latest `AutoTarget-v*.zip` from the
   [Releases](../../releases) page.
2. Extract `version.dll` and `AutoTarget.ini` into your WoW directory, next to
   `WoW.exe`.
3. **If you already have a `version.dll`** in your WoW folder (rare — almost no
   mod ships one): rename the existing one to **`version_chain.dll`** first,
   *then* drop ours in as `version.dll`. AutoTarget chains through it so both
   stay alive. Most users have no pre-existing `version.dll` and can skip this
   step.
4. Open `AutoTarget.ini`, change `Mode = off` to `Mode = live`, save.
5. Launch the game. A first run writes `AutoTarget.log` next to the DLL — the
   top of the file confirms the version and a successful client match.

**To uninstall:** delete `version.dll` and `AutoTarget.ini`. If you had a
`version_chain.dll`, rename it back to `version.dll`.

### Mage / caster macro examples

```
/cast [target=mouseover,exists,harm] Fireball; Fireball
/cast [target=mouseover,exists,harm] Frostbolt; Frostbolt
/cast [target=mouseover,exists,harm,nodead] Polymorph
```

The `,exists,harm` clause keeps casts safe when nothing is in your cone
(falls through to the default behaviour instead of erroring).

---

## Build from source

```
cd client
cmake -B build -A Win32
cmake --build build --config Release
```

Outputs:

- `build/Release/version.dll` — the proxy DLL.
- `build/Release/autotarget_tests.exe` — the engine unit tests.

Run the tests with `ctest --test-dir build -C Release` (or invoke the exe
directly). MinHook is fetched automatically by CMake — no manual setup.

**Reproducible builds.** A paranoid user can build this themselves and compare
the resulting DLL against any release binary. Commit hash, MSVC version, and
build flags are deterministic enough that byte-level diffs work for a sanity
check; signed Releases are the source of truth.

---

## Configuration

Everything tunable lives in `AutoTarget.ini`, inline-documented. Highlights:

| Key | Effect |
| --- | --- |
| `Mode` | `off` / `diagnostic` / `live`. Ship default is `off` so a first launch is a no-op. |
| `EnabledOnStartup` | Whether the DLL begins each launch enabled. |
| `TickRateMs` | Engine evaluation cadence (default 70 ms). |
| `LogLevel` | `error` / `warn` / `info` / `debug`. |
| `tier1.RangeYards` | Brawl bubble radius. Default 2.5. |
| `tier2.RangeYards` | Aimed-cone range. Default 40 yd (caster max). |
| `tier2.ConeDegrees` | Aimed-cone width. Default 30 (±15°). Narrower = stricter aim. |
| `behaviour.Mechanism` | `action` (default — Action Targeting via spell hook), `mouseover` (drive mouseover slot, needs macros), or `target` (commit hard target every tick). ActionTarget commits the aim pick on every offensive cast; no per-class or per-spell config. |
| `behaviour.AggressiveReaim` | Re-aim the pick to your camera while in combat. |
| `behaviour.AimOffsetDegrees` | Degrees added to your facing if the cone sits off-axis. Should be 0 in mouselook. |
| `input.ToggleHotkeyVK` | Virtual-key code for the on/off hotkey. Default `0x79` = F10. |

Edit the file and relaunch the client to apply.

### Recommended first-run sequence

1. **`Mode = off`** — confirm the client still launches and runs normally with
   the DLL in place.
2. **`Mode = diagnostic`** — the engine runs and writes its picks to the log
   but never touches the mouseover slot. Read the log to verify the offsets
   are correct for your client (see *Offset verification* below).
3. **`Mode = live`** — full operation.

---

## On/off toggle

Two paths drive the same master enable:

- **Hotkey** — F10 by default, set via `ToggleHotkeyVK` in the ini.
- **In-game checkbox + `/at` command** — appears in *Interface Options →
  Combat*, with `/at on`, `/at off`, and `/at` (status). Requires
  `kFnFrameScriptRegisterFunction` to be set in `Offsets.h` (see below). When
  unset, the UI is skipped and only the hotkey is wired up.

The startup state is `EnabledOnStartup` in the ini.

---

## Offset verification

The 12340 client has been frozen since 2010, so its memory offsets are stable.
`src/GameInterface/Offsets.h` is the single source of truth, partitioned by
confidence:

- **[CONFIRMED]** — verified against AzerothCore's `UpdateFields.h` and/or
  multiple independent sources, and behaviour-confirmed in-client.
- **[VERIFY]** — single-sourced or sources disagree. They work if correct and
  fail safely if not.

| Offset | Used for | Symptom if wrong |
| --- | --- | --- |
| `kUnitPos*Offset` | unit & player position | wrong picks, or none — verified empirically in v0.1.0 |
| `kFnTargetUnit` | committing the hard target (action / target mechanisms) | commit does nothing or faults |
| `kFnSpellCastSpell` | Action Targeting cast hook | MinHook install fails, mod falls back to mouseover behaviour |
| `kStaticMouseoverGuid` | mouseover slot write | mouseover mechanism does nothing |
| `kStaticTargetGuid` | reading the current target | re-aim misbehaves |
| `kFnFrameScriptRegisterFunction` | in-game checkbox / `/at` bridge | UI is skipped, hotkey still works |
| `kCamera*` | dedicated camera read (currently unused) | falls back to model facing — safe in mouselook |

A wrong address never crashes the game: every tick and hook callback runs under
a structured-exception guard. If a fault fires, AutoTarget self-disables and
logs it; `WoW.exe` keeps running.

---

## Safety

- **Image check** — AutoTarget verifies it is inside `WoW.exe` at the expected
  image base before reading a single offset. A mismatch self-disables it.
- **Crash containment** — every per-tick body and hook callback runs through
  `SafeMode`. A fault disables AutoTarget and logs it. It never takes the
  client down.
- **Module pinning** — the DLL pins itself in the process so a Windows-driven
  unload (which would point hooks at freed memory) cannot happen.
- **Kill switch** — the hotkey (and, when active, the checkbox / `/at`) halts
  all writes instantly. `Mode = off` in the ini takes effect at launch.

---

## For realm operators

AutoTarget is a client-side modification — it cannot be detected or
disabled server-side. If your realm runs Warden, consider whitelisting
`version.dll`; the DLL is unobfuscated and its behaviour can be audited line by
line in this repository. The community-facing rationale is the same as for any
other graphical wrapper: it modifies client behaviour, not network behaviour.

---

## Architecture

Strict one-way dependencies; the engine is pure logic and unit-tested
off-client.

```
Bootstrap  -->  Orchestration  -->  Engine          (pure, no client deps)
                      |
                      +---------->  GameInterface   (all offsets quarantined here)
Config, Input, Diagnostics are cross-cutting.
```

- `Engine/` — cone model, scoring, soft-target tracking, state machine, commit
  policy. No client dependency.
- `GameInterface/` — the only layer that knows offsets: object manager, unit
  accessors, selection, line-of-sight, MinHook detours (FrameHook, SelectionHook).
- `Orchestration/` — the per-tick controller wiring engine and game together.
- `Bootstrap/` — `DllMain`, the version.dll proxy exports, version stamp, initializer.
- `Config/`, `Input/`, `Diagnostics/` — cross-cutting support.

---

## License

MIT — see [`LICENSE`](../LICENSE) at the repo root. MinHook is vendored under
its own MIT license.
