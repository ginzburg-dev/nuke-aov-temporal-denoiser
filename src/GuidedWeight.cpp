#include "ntd/GuidedWeight.h"

#include <cmath>
#include <limits>

namespace ntd {
namespace {

float squared(float value) noexcept { return value * value; }

float normalized(float squaredDistanceValue, float sigma) noexcept {
    return squaredDistanceValue / squared(sigma);
}

}  // namespace

bool isFinite(const PixelGuidance& guidance) noexcept {
    return isFinite(guidance.beauty) && isFinite(guidance.albedo) && isFinite(guidance.normal)
        && isFinite(guidance.position) && std::isfinite(guidance.depth);
}

float guidanceDistanceSquared(const PixelGuidance& reference, const PixelGuidance& candidate,
    const DenoiseParameters& parameters) noexcept {
    if (!isFinite(reference) || !isFinite(candidate) || !parameters.isValid()) {
        return std::numeric_limits<float>::infinity();
    }

    const float depthDelta = reference.depth - candidate.depth;
    return normalized(squaredDistance(reference.beauty, candidate.beauty), parameters.beautySigma)
        + normalized(squaredDistance(reference.albedo, candidate.albedo), parameters.albedoSigma)
        + normalized(squaredDistance(reference.normal, candidate.normal), parameters.normalSigma)
        + normalized(
            squaredDistance(reference.position, candidate.position), parameters.positionSigma)
        + normalized(squared(depthDelta), parameters.depthSigma);
}

float computeSpatialWeight(const PixelGuidance& reference, const PixelGuidance& candidate,
    float pixelDistanceSquared, const DenoiseParameters& parameters) noexcept {
    if (!std::isfinite(pixelDistanceSquared) || pixelDistanceSquared < 0.0F) {
        return 0.0F;
    }

    const float guidanceCost = guidanceDistanceSquared(reference, candidate, parameters);
    if (!std::isfinite(guidanceCost)) {
        return 0.0F;
    }

    const float spatialCost = normalized(pixelDistanceSquared, parameters.spatialSigma);
    return parameters.spatialWeight * std::exp(-0.5F * (guidanceCost + spatialCost));
}

float computeTemporalWeight(const PixelGuidance& reference, const PixelGuidance& candidate,
    float searchDistanceSquared, int frameDistance, const DenoiseParameters& parameters) noexcept {
    if (!passesTemporalRejection(reference, candidate, parameters)
        || !std::isfinite(searchDistanceSquared) || searchDistanceSquared < 0.0F
        || frameDistance <= 0) {
        return 0.0F;
    }

    const float guidanceCost = guidanceDistanceSquared(reference, candidate, parameters);
    const float searchCost = normalized(searchDistanceSquared, parameters.spatialSigma);
    const float timeCost = parameters.frameDecay * static_cast<float>(frameDistance - 1);
    return parameters.temporalWeight * std::exp(-0.5F * (guidanceCost + searchCost) - timeCost);
}

bool passesTemporalRejection(const PixelGuidance& reference, const PixelGuidance& candidate,
    const DenoiseParameters& parameters) noexcept {
    if (!isFinite(reference) || !isFinite(candidate) || !parameters.isValid()) {
        return false;
    }

    return squaredDistance(reference.beauty, candidate.beauty)
        <= squared(parameters.matchBeautyThreshold)
        && squaredDistance(reference.albedo, candidate.albedo)
        <= squared(parameters.matchAlbedoThreshold)
        && squaredDistance(reference.position, candidate.position)
        <= squared(parameters.matchPositionThreshold);
}

}  // namespace ntd
