#include "Config/ConfigManager.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>

namespace autotarget {

namespace {

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

using KeyMap = std::map<std::string, std::string>;

// Parses a simple sectioned INI into "section.key" -> value (keys lowercased).
bool ParseIni(const std::string& path, KeyMap& out) {
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    std::string line;
    std::string section;
    while (std::getline(f, line)) {
        const std::string t = Trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';')
            continue;
        if (t.front() == '[' && t.back() == ']') {
            section = Lower(Trim(t.substr(1, t.size() - 2)));
            continue;
        }
        const size_t eq = t.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = Lower(Trim(t.substr(0, eq)));
        const std::string val = Trim(t.substr(eq + 1));
        if (!key.empty())
            out[section + "." + key] = val;
    }
    return true;
}

bool ParseBool(const std::string& v, bool def) {
    const std::string s = Lower(v);
    if (s == "true" || s == "1" || s == "yes" || s == "on")
        return true;
    if (s == "false" || s == "0" || s == "no" || s == "off")
        return false;
    return def;
}

float ParseFloat(const std::string& v, float def) {
    try {
        return std::stof(v);
    } catch (...) {
        return def;
    }
}

// Accepts decimal and 0x-prefixed hex (base 0).
long ParseLong(const std::string& v, long def) {
    char* end = nullptr;
    const long result = std::strtol(v.c_str(), &end, 0);
    return (end == v.c_str()) ? def : result;
}

} // namespace

bool ConfigManager::Load(const std::string& iniPath) {
    KeyMap m;
    if (!ParseIni(iniPath, m)) {
        AT_LOG_WARN("Config: '%s' not found - using defaults", iniPath.c_str());
        return false;
    }

    const auto get = [&](const char* k) -> const std::string* {
        const auto it = m.find(k);
        return it == m.end() ? nullptr : &it->second;
    };

    if (auto* v = get("general.tickratems"))
        cfg_.tickRateMs = static_cast<std::uint32_t>(ParseLong(*v, cfg_.tickRateMs));
    if (auto* v = get("general.loglevel"))
        cfg_.logLevel = Logger::ParseLevel(*v);
    if (auto* v = get("general.enabledonstartup"))
        cfg_.enabledOnStartup = ParseBool(*v, cfg_.enabledOnStartup);
    if (auto* v = get("general.mode")) {
        const std::string m = Lower(*v);
        if (m == "off")            cfg_.mode = RunMode::Off;
        else if (m == "live")      cfg_.mode = RunMode::Live;
        else                       cfg_.mode = RunMode::Diagnostic;
    }
    if (auto* v = get("behaviour.mechanism")) {
        const std::string m = Lower(*v);
        if (m == "target" || m == "hardtarget")
            cfg_.mechanism = Mechanism::HardTarget;
        else if (m == "action" || m == "actiontarget")
            cfg_.mechanism = Mechanism::ActionTarget;
        else
            cfg_.mechanism = Mechanism::Mouseover;
    }

    // [offensive] SpellIds was removed in v0.2.1 - aim now wins every cast
    // unconditionally. Any stale ini section is silently ignored.

    if (auto* v = get("tier1.rangeyards"))
        cfg_.tier1RangeYards = ParseFloat(*v, cfg_.tier1RangeYards);
    if (auto* v = get("tier1.conedegrees"))
        cfg_.tier1ConeDegrees = ParseFloat(*v, cfg_.tier1ConeDegrees);
    if (auto* v = get("tier2.rangeyards"))
        cfg_.tier2RangeYards = ParseFloat(*v, cfg_.tier2RangeYards);
    if (auto* v = get("tier2.conedegrees"))
        cfg_.tier2ConeDegrees = ParseFloat(*v, cfg_.tier2ConeDegrees);

    if (auto* v = get("behaviour.aggressivereaim"))
        cfg_.aggressiveReaim = ParseBool(*v, cfg_.aggressiveReaim);
    if (auto* v = get("behaviour.hysteresisbonus"))
        cfg_.hysteresisBonus = ParseFloat(*v, cfg_.hysteresisBonus);
    if (auto* v = get("behaviour.softtargetgracems"))
        cfg_.softTargetGraceMs = static_cast<std::uint32_t>(ParseLong(*v, cfg_.softTargetGraceMs));
    if (auto* v = get("behaviour.softcommitdwellms"))
        cfg_.softCommitDwellMs = static_cast<std::uint32_t>(ParseLong(*v, cfg_.softCommitDwellMs));
    if (auto* v = get("behaviour.aimoffsetdegrees"))
        cfg_.aimOffsetDegrees = ParseFloat(*v, cfg_.aimOffsetDegrees);
    if (auto* v = get("behaviour.ignorecritters"))
        cfg_.ignoreCritters = ParseBool(*v, cfg_.ignoreCritters);
    if (auto* v = get("behaviour.smartunstick"))
        cfg_.smartUnstick = ParseBool(*v, cfg_.smartUnstick);
    if (auto* v = get("behaviour.smartunstickmaxrangeyards"))
        cfg_.smartUnstickMaxRangeYards = ParseFloat(*v, cfg_.smartUnstickMaxRangeYards);

    // [input] section removed in v0.3.5 - the toggle hotkey is gone. Toggle
    // AutoTarget via the in-game checkbox in Interface Options > Combat, or
    // the /at chat command.

    // Clamp to sane ranges so a bad edit cannot break the engine.
    if (cfg_.tickRateMs < 16)   cfg_.tickRateMs = 16;
    if (cfg_.tickRateMs > 1000) cfg_.tickRateMs = 1000;
    if (cfg_.tier1ConeDegrees < 0.0f)   cfg_.tier1ConeDegrees = 0.0f;
    if (cfg_.tier1ConeDegrees > 360.0f) cfg_.tier1ConeDegrees = 360.0f;
    if (cfg_.tier2ConeDegrees < 0.0f)   cfg_.tier2ConeDegrees = 0.0f;
    if (cfg_.tier2ConeDegrees > 360.0f) cfg_.tier2ConeDegrees = 360.0f;
    if (cfg_.tier1RangeYards < 0.0f) cfg_.tier1RangeYards = 0.0f;
    if (cfg_.tier2RangeYards < 0.0f) cfg_.tier2RangeYards = 0.0f;
    if (cfg_.hysteresisBonus < 0.0f) cfg_.hysteresisBonus = 0.0f;
    if (cfg_.hysteresisBonus > 1.0f) cfg_.hysteresisBonus = 1.0f;

    AT_LOG_INFO("Config: loaded from '%s'", iniPath.c_str());
    return true;
}

EngineConfig ConfigManager::ToEngineConfig() const {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    EngineConfig e{};
    e.tier1Range = cfg_.tier1RangeYards;
    e.tier1HalfAngleRad = (cfg_.tier1ConeDegrees * 0.5f) * kDegToRad;
    e.tier2Range = cfg_.tier2RangeYards;
    e.tier2HalfAngleRad = (cfg_.tier2ConeDegrees * 0.5f) * kDegToRad;
    e.hysteresisBonus = cfg_.hysteresisBonus;
    e.aggressiveReaim = cfg_.aggressiveReaim;
    e.softTargetGraceMs = cfg_.softTargetGraceMs;
    e.softCommitDwellMs = cfg_.softCommitDwellMs;
    return e;
}

} // namespace autotarget
