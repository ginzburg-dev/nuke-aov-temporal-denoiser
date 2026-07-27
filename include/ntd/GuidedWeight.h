#pragma once

#include "ntd/DenoiseParameters.h"
#include "ntd/Math.h"

namespace ntd {

struct PixelGuidance {
    Vec3f beauty;
    Vec3f albedo;
    Vec3f normal;
    Vec3f position;
    float depth = 0.0F;
};

[[nodiscard]] bool isFinite(const PixelGuidance& guidance) noexcept;

[[nodiscard]] float guidanceDistanceSquared(const PixelGuidance& reference,
    const PixelGuidance& candidate, const DenoiseParameters& parameters) noexcept;

[[nodiscard]] float computeSpatialWeight(const PixelGuidance& reference,
    const PixelGuidance& candidate, float pixelDistanceSquared,
    const DenoiseParameters& parameters) noexcept;

[[nodiscard]] float computeTemporalWeight(const PixelGuidance& reference,
    const PixelGuidance& candidate, float searchDistanceSquared, int frameDistance,
    const DenoiseParameters& parameters) noexcept;

[[nodiscard]] bool passesTemporalRejection(const PixelGuidance& reference,
    const PixelGuidance& candidate, const DenoiseParameters& parameters) noexcept;

}  // namespace ntd
