#pragma once

#include <cstdint>
#include <string>

#include "Diagnostics/Logger.h"
#include "Engine/EngineTypes.h"

namespace autotarget {

// How much of AutoTarget runs.
enum class RunMode {
    Off,        // DLL loads and forwards d3d9 only - installs no hook at all.
    Diagnostic, // frame hook + engine, reads and logs, never writes a target.
    Live        // full: selection hook, in-game UI, target writes.
};

// How the engine's soft pick is delivered to the player.
enum class Mechanism {
    Mouseover,    // drive the client's mouseover slot; the real target is never
                  // touched. Abilities follow it via /cast [@mouseover] macros.
    HardTarget,   // commit the soft pick straight into the real target every tick.
    ActionTarget  // soft pick is invisible; commits to the real target only when
                  // the player casts an offensive spell with no valid target.
                  // Plain /cast Fireball works - no mouseover macros required.
                  // This is Blizzard's Action Targeting behaviour.
};

// All tuning values. The on/off enable state is deliberately NOT here — it is a
// saved CVar driven by the "Enable AutoTarget" checkbox so players can toggle it
// in-game and have it persist.
struct Config {
    std::uint32_t tickRateMs = 70;
    LogLevel      logLevel = LogLevel::Info;

    // Master enable state the DLL starts with. The in-game checkbox and the
    // hotkey flip it at runtime; this is only the value used before the player
    // touches either (3.3.5a has no Lua-writable custom CVar to persist it).
    bool enabledOnStartup = true;

    // How much of AutoTarget runs. See RunMode above.
    RunMode mode = RunMode::Diagnostic;

    // How the soft pick is delivered. See Mechanism above.
    Mechanism mechanism = Mechanism::Mouseover;

    float tier1RangeYards  = 8.0f;
    float tier1ConeDegrees = 150.0f; // full cone angle
    float tier2RangeYards  = 30.0f;
    float tier2ConeDegrees = 36.0f;  // full cone angle

    bool          aggressiveReaim   = true;
    float         hysteresisBonus   = 0.20f;
    std::uint32_t softTargetGraceMs = 400;
    std::uint32_t softCommitDwellMs = 200; // ms a challenger must lead before stealing the soft pick

    // Degrees added to the player facing before the cone is measured. Use it to
    // dial out a constant left/right bias in what the cone selects.
    float aimOffsetDegrees = 0.0f;

    // Skip critters (rabbits and the like). Off by default: the health-based
    // critter guess also catches training dummies, and a stray critter as the
    // soft target is harmless.
    bool ignoreCritters = false;

    // SmartUnstick (v0.3.4). When the active target slot holds a GUID that is
    // demonstrably unusable - out of cast range, server-leashed, or sitting
    // 180 degrees behind the player while a closer enemy is in the aim cone -
    // clear the slot so the next cast acquires the soft pick instead of
    // erroring out. Dead/Missing handling is always on regardless of this
    // flag (shipped in v0.3.3); SmartUnstick gates only the new reasons.
    bool  smartUnstick              = true;
    float smartUnstickMaxRangeYards = 40.0f; // WoW hard cap for any cast

};

// Loads and parses AutoTarget.ini.
class ConfigManager {
public:
    // Returns true if the file was found and parsed; false means defaults are in
    // effect. Get() is valid either way.
    bool Load(const std::string& iniPath);

    const Config& Get() const { return cfg_; }

    // Tuning the engine consumes, converted to its units
    // (full cone degrees -> half-angle radians).
    EngineConfig ToEngineConfig() const;

private:
    Config cfg_;
};

} // namespace autotarget
