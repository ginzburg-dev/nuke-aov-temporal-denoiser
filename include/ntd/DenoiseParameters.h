#pragma once

#include <algorithm>
#include <cmath>

namespace ntd {

struct DenoiseParameters {
    static constexpr float kMinimumSigma = 1.0e-4F;
    static constexpr int kMaximumRadius = 64;
    static constexpr int kMaximumTemporalRadius = 3;

    int spatialRadius = 5;
    int searchRadius = 3;
    int temporalRadius = 3;

    float spatialWeight = 0.7F;
    float temporalWeight = 0.3F;
    float frameDecay = 0.35F;
    float motionScale = 1.0F;
    float maximumMotionPerFrame = 32.0F;

    float beautySigma = 0.2F;
    float albedoSigma = 0.1F;
    float normalSigma = 0.2F;
    float depthSigma = 1.0F;
    float positionSigma = 1.0F;
    float spatialSigma = 3.0F;

    float matchBeautyThreshold = 0.5F;
    float matchAlbedoThreshold = 0.25F;
    float matchPositionThreshold = 2.0F;

    void sanitize() noexcept {
        spatialRadius = std::clamp(spatialRadius, 0, kMaximumRadius);
        searchRadius = std::clamp(searchRadius, 0, kMaximumRadius);
        temporalRadius = std::clamp(temporalRadius, 0, kMaximumTemporalRadius);

        spatialWeight = sanitizeNonNegative(spatialWeight);
        temporalWeight = sanitizeNonNegative(temporalWeight);
        frameDecay = sanitizeNonNegative(frameDecay);
        motionScale = std::isfinite(motionScale) ? motionScale : 1.0F;
        maximumMotionPerFrame = std::min(4096.0F, sanitizeNonNegative(maximumMotionPerFrame));

        beautySigma = sanitizeSigma(beautySigma);
        albedoSigma = sanitizeSigma(albedoSigma);
        normalSigma = sanitizeSigma(normalSigma);
        depthSigma = sanitizeSigma(depthSigma);
        positionSigma = sanitizeSigma(positionSigma);
        spatialSigma = sanitizeSigma(spatialSigma);

        matchBeautyThreshold = sanitizeNonNegative(matchBeautyThreshold);
        matchAlbedoThreshold = sanitizeNonNegative(matchAlbedoThreshold);
        matchPositionThreshold = sanitizeNonNegative(matchPositionThreshold);
    }

    [[nodiscard]] bool isValid() const noexcept {
        return spatialRadius >= 0 && spatialRadius <= kMaximumRadius && searchRadius >= 0
            && searchRadius <= kMaximumRadius && temporalRadius >= 0
            && temporalRadius <= kMaximumTemporalRadius && finiteNonNegative(spatialWeight)
            && finiteNonNegative(temporalWeight) && finiteNonNegative(frameDecay)
            && std::isfinite(motionScale) && finiteNonNegative(maximumMotionPerFrame)
            && validSigma(beautySigma) && validSigma(albedoSigma) && validSigma(normalSigma)
            && validSigma(depthSigma) && validSigma(positionSigma) && validSigma(spatialSigma)
            && finiteNonNegative(matchBeautyThreshold) && finiteNonNegative(matchAlbedoThreshold)
            && finiteNonNegative(matchPositionThreshold);
    }

    [[nodiscard]] int requestPadding() const noexcept {
        if (!isValid()) {
            return 0;
        }
        const float temporalMotion = maximumMotionPerFrame * static_cast<float>(temporalRadius);
        const int temporalPadding = searchRadius + static_cast<int>(std::ceil(temporalMotion));
        return std::max(spatialRadius, temporalPadding);
    }

private:
    static float sanitizeSigma(float value) noexcept {
        if (!std::isfinite(value)) {
            return 1.0F;
        }
        return std::max(kMinimumSigma, value);
    }

    static float sanitizeNonNegative(float value) noexcept {
        return std::isfinite(value) ? std::max(0.0F, value) : 0.0F;
    }

    static bool finiteNonNegative(float value) noexcept {
        return std::isfinite(value) && value >= 0.0F;
    }

    static bool validSigma(float value) noexcept {
        return std::isfinite(value) && value >= kMinimumSigma;
    }
};

}  // namespace ntd
