#pragma once

#include "ntd/GuidedWeight.h"

#include <cstddef>
#include <limits>
#include <vector>

namespace ntd {

struct MatchCandidate {
    Vec2f searchOffset;
    PixelGuidance guidance;
};

struct MatchResult {
    std::size_t index = 0;
    float cost = std::numeric_limits<float>::infinity();
    bool found = false;
};

[[nodiscard]] float temporalMatchCost(const PixelGuidance& reference,
    const MatchCandidate& candidate, const DenoiseParameters& parameters) noexcept;

[[nodiscard]] MatchResult findBestTemporalMatch(const PixelGuidance& reference,
    const std::vector<MatchCandidate>& candidates, const DenoiseParameters& parameters) noexcept;

}  // namespace ntd
