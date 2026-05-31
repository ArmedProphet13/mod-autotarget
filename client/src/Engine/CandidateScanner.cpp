#include "CandidateScanner.h"

#include <algorithm>

namespace autotarget {

CandidateScanner::CandidateScanner(const EngineConfig& cfg)
    : cfg_(cfg), cone_(cfg), scorer_(cfg) {}

std::vector<ScoredCandidate> CandidateScanner::Scan(const TargetingSnapshot& snap) const {
    std::vector<ScoredCandidate> ranked;
    ranked.reserve(snap.units.size());

    for (const UnitInfo& u : snap.units) {
        if (u.guid == kNoGuid)
            continue;
        if (!u.alive || !u.attackable || u.critter || !u.lineOfSight)
            continue;

        const ConeModel::Geometry g = cone_.Classify(snap, u);
        if (g.tier == Tier::None)
            continue;

        ScoredCandidate c{};
        c.guid = u.guid;
        c.tier = g.tier;
        c.distance = g.distance;
        c.angleOffset = g.angleOffset;
        c.score = scorer_.Score(g.tier, g.distance, g.angleOffset,
                                u.guid, snap.currentTarget,
                                snap.previousSoftTarget);
        ranked.push_back(c);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const ScoredCandidate& a, const ScoredCandidate& b) {
                  if (a.score != b.score)
                      return a.score > b.score;
                  return a.guid < b.guid; // stable tiebreak for determinism
              });
    return ranked;
}

Guid CandidateScanner::Best(const std::vector<ScoredCandidate>& ranked) {
    return ranked.empty() ? kNoGuid : ranked.front().guid;
}

} // namespace autotarget
