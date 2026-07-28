#include "ntd/DenoiseParameters.h"
#include "ntd/GuidedWeight.h"
#include "ntd/TemporalMatcher.h"
#include "ntd/WeightedAccumulator.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool nearlyEqual(float lhs, float rhs, float tolerance = 1.0e-5F) {
    return std::fabs(lhs - rhs) <= tolerance;
}

ntd::PixelGuidance makeGuidance(float value = 0.0F) {
    const ntd::Vec3f vector{value, value, value};
    return {vector, vector, vector, vector, value};
}

void parametersAreSafe() {
    ntd::DenoiseParameters parameters;
    check(parameters.isValid(), "default parameters must be valid");
    check(parameters.requestPadding()
            == parameters.searchRadius
                + static_cast<int>(std::ceil(parameters.maximumMotionPerFrame
                    * static_cast<float>(parameters.temporalRadius))),
        "request padding must cover motion and correspondence search");

    parameters.temporalRadius = 99;
    parameters.beautySigma = 0.0F;
    parameters.normalSigma = std::numeric_limits<float>::quiet_NaN();
    parameters.spatialWeight = -1.0F;
    parameters.maximumMotionPerFrame = std::numeric_limits<float>::quiet_NaN();
    parameters.sanitize();

    check(parameters.isValid(), "sanitized parameters must be valid");
    check(parameters.temporalRadius == 3, "temporal radius must be bounded");
    check(parameters.beautySigma >= ntd::DenoiseParameters::kMinimumSigma,
        "zero sigma must not survive sanitization");
    check(parameters.spatialWeight == 0.0F, "negative weights must be clamped");
    check(parameters.maximumMotionPerFrame == 0.0F, "non-finite motion bounds must be made safe");
    check(parameters.requestPadding() == parameters.spatialRadius,
        "spatial radius must determine padding when motion is disabled");
}

void spatialWeightsRespectGuides() {
    const ntd::DenoiseParameters parameters;
    const ntd::PixelGuidance reference = makeGuidance();
    ntd::PixelGuidance edge = reference;
    edge.normal = {0.2F, 0.0F, 0.0F};

    const float identical = ntd::computeSpatialWeight(reference, reference, 0.0F, parameters);
    const float acrossEdge = ntd::computeSpatialWeight(reference, edge, 0.0F, parameters);
    const float farther = ntd::computeSpatialWeight(reference, reference, 9.0F, parameters);

    check(nearlyEqual(identical, parameters.spatialWeight),
        "identical samples must receive the full spatial weight");
    check(acrossEdge < identical, "normal discontinuities must reduce weight");
    check(farther < identical, "pixel distance must reduce weight");
}

void matcherSelectsTheBestValidCandidate() {
    const ntd::DenoiseParameters parameters;
    const ntd::PixelGuidance reference = makeGuidance();

    ntd::MatchCandidate rejected{{0.0F, 0.0F}, makeGuidance(10.0F)};
    ntd::MatchCandidate validButFar{{2.0F, 0.0F}, makeGuidance(0.02F)};
    ntd::MatchCandidate best{{0.0F, 0.0F}, makeGuidance(0.01F)};

    const std::vector<ntd::MatchCandidate> candidates{rejected, validButFar, best};
    const ntd::MatchResult result = ntd::findBestTemporalMatch(reference, candidates, parameters);

    check(result.found, "matcher must find a valid candidate");
    check(result.index == 2, "matcher must select the lowest-cost candidate");
}

void temporalSearchUsesAnExactLowerBound() {
    ntd::DenoiseParameters parameters;
    parameters.spatialSigma = 2.0F;

    check(ntd::canStopTemporalSearch(0.25F, 1, parameters),
        "an equal lower bound cannot improve the current match");
    check(!ntd::canStopTemporalSearch(0.26F, 1, parameters),
        "search must continue while the next ring can improve the match");
    check(ntd::canStopTemporalSearch(0.9F, 2, parameters),
        "a farther ring must use its larger distance lower bound");
    check(!ntd::canStopTemporalSearch(
            std::numeric_limits<float>::infinity(), 1, parameters),
        "search must not stop before finding a finite match");
}

void temporalRejectionStopsGhosts() {
    const ntd::DenoiseParameters parameters;
    const ntd::PixelGuidance reference = makeGuidance();
    ntd::PixelGuidance disocclusion = reference;
    disocclusion.position = {100.0F, 0.0F, 0.0F};

    check(ntd::computeTemporalWeight(reference, disocclusion, 0.0F, 1, parameters) == 0.0F,
        "a position discontinuity must be rejected");
    check(ntd::computeTemporalWeight(reference, reference, 0.0F, 2, parameters)
            < ntd::computeTemporalWeight(reference, reference, 0.0F, 1, parameters),
        "older frames must decay");
}

void accumulatorNormalizesMultiplePasses() {
    ntd::WeightedAccumulator<2> accumulator;
    accumulator.add({ntd::Vec3f{1.0F, 2.0F, 3.0F}, ntd::Vec3f{10.0F, 20.0F, 30.0F}}, 1.0F);
    accumulator.add({ntd::Vec3f{3.0F, 4.0F, 5.0F}, ntd::Vec3f{30.0F, 40.0F, 50.0F}}, 1.0F);

    const auto value = accumulator.value();
    check(nearlyEqual(value[0].x, 2.0F) && nearlyEqual(value[0].z, 4.0F),
        "beauty pass must be normalized");
    check(nearlyEqual(value[1].x, 20.0F) && nearlyEqual(value[1].z, 40.0F),
        "auxiliary passes must use the same weight");
}

}  // namespace

int main() {
    parametersAreSafe();
    spatialWeightsRespectGuides();
    matcherSelectsTheBestValidCandidate();
    temporalSearchUsesAnExactLowerBound();
    temporalRejectionStopsGhosts();
    accumulatorNormalizesMultiplePasses();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core tests passed\n";
    return EXIT_SUCCESS;
}
