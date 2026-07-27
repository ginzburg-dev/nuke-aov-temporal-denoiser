#include "ntd/TemporalMatcher.h"

#include <cmath>
#include <limits>

namespace ntd {

float temporalMatchCost(const PixelGuidance& reference, const MatchCandidate& candidate,
    const DenoiseParameters& parameters) noexcept {
    if (!isFinite(candidate.searchOffset)
        || !passesTemporalRejection(reference, candidate.guidance, parameters)) {
        return std::numeric_limits<float>::infinity();
    }

    return guidanceDistanceSquared(reference, candidate.guidance, parameters)
        + squaredLength(candidate.searchOffset)
        / (parameters.spatialSigma * parameters.spatialSigma);
}

MatchResult findBestTemporalMatch(const PixelGuidance& reference,
    const std::vector<MatchCandidate>& candidates, const DenoiseParameters& parameters) noexcept {
    MatchResult result;

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const MatchCandidate& candidate = candidates[index];
        const float cost = temporalMatchCost(reference, candidate, parameters);
        if (std::isfinite(cost) && cost < result.cost) {
            result.index = index;
            result.cost = cost;
            result.found = true;
        }
    }

    return result;
}

}  // namespace ntd
