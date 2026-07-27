#include "ntd/DenoiseParameters.h"
#include "ntd/GuidedWeight.h"
#include "ntd/TemporalMatcher.h"
#include "ntd/WeightedAccumulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kWidth = 48;
constexpr int kHeight = 30;
constexpr int kPanelScale = 4;

struct Frame {
    std::vector<ntd::PixelGuidance> guidance;
    std::vector<ntd::Vec3f> beauty;
};

int indexOf(int x, int y) { return y * kWidth + x; }

bool isObject(int x, int y, int frameOffset) {
    const int shiftedLeft = 15 + frameOffset;
    const int shiftedRight = 32 + frameOffset;
    return x >= shiftedLeft && x < shiftedRight && y >= 7 && y < 24;
}

ntd::Vec3f referenceColor(bool object) {
    return object ? ntd::Vec3f{0.95F, 0.34F, 0.08F} : ntd::Vec3f{0.04F, 0.10F, 0.20F};
}

float randomSigned(int x, int y, int frameOffset, std::uint32_t channel) {
    std::uint32_t value = static_cast<std::uint32_t>(x + 101) * 0x9E3779B9U
        ^ static_cast<std::uint32_t>(y + 211) * 0x85EBCA6BU
        ^ static_cast<std::uint32_t>(frameOffset + 17) * 0xC2B2AE35U ^ channel * 0x27D4EB2FU;
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    const float unit = static_cast<float>(value & 0x00FFFFFFU) / 16777215.0F;
    return unit * 2.0F - 1.0F;
}

Frame makeFrame(int frameOffset) {
    Frame frame;
    frame.guidance.reserve(static_cast<std::size_t>(kWidth * kHeight));
    frame.beauty.reserve(static_cast<std::size_t>(kWidth * kHeight));

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const bool object = isObject(x, y, frameOffset);
            const ntd::Vec3f clean = referenceColor(object);
            const float noiseAmplitude = object ? 0.28F : 0.18F;
            const ntd::Vec3f noisy{
                clean.x + noiseAmplitude * randomSigned(x, y, frameOffset, 0),
                clean.y + noiseAmplitude * randomSigned(x, y, frameOffset, 1),
                clean.z + noiseAmplitude * randomSigned(x, y, frameOffset, 2),
            };

            const float stableX =
                static_cast<float>(object ? x - frameOffset : x) / static_cast<float>(kWidth);
            const float normalizedY = static_cast<float>(y) / static_cast<float>(kHeight);
            frame.beauty.push_back(noisy);
            frame.guidance.push_back({
                noisy,
                clean,
                object ? ntd::Vec3f{0.0F, 1.0F, 0.0F} : ntd::Vec3f{0.0F, 0.0F, 1.0F},
                {stableX, normalizedY, object ? 1.0F : 0.0F},
                object ? 4.0F : 10.0F,
            });
        }
    }

    return frame;
}

ntd::DenoiseParameters demoParameters() {
    ntd::DenoiseParameters parameters;
    parameters.spatialRadius = 2;
    parameters.searchRadius = 1;
    parameters.temporalRadius = 3;
    parameters.spatialWeight = 0.35F;
    parameters.temporalWeight = 0.9F;
    parameters.frameDecay = 0.12F;
    parameters.beautySigma = 0.45F;
    parameters.albedoSigma = 0.08F;
    parameters.normalSigma = 0.2F;
    parameters.depthSigma = 0.5F;
    parameters.positionSigma = 0.08F;
    parameters.spatialSigma = 2.0F;
    parameters.matchBeautyThreshold = 0.9F;
    parameters.matchAlbedoThreshold = 0.2F;
    parameters.matchPositionThreshold = 0.12F;
    parameters.sanitize();
    return parameters;
}

ntd::Vec3f sampleBeauty(const Frame& frame, int x, int y) {
    const int safeX = std::clamp(x, 0, kWidth - 1);
    const int safeY = std::clamp(y, 0, kHeight - 1);
    return frame.beauty[static_cast<std::size_t>(indexOf(safeX, safeY))];
}

const ntd::PixelGuidance& sampleGuidance(const Frame& frame, int x, int y) {
    const int safeX = std::clamp(x, 0, kWidth - 1);
    const int safeY = std::clamp(y, 0, kHeight - 1);
    return frame.guidance[static_cast<std::size_t>(indexOf(safeX, safeY))];
}

std::vector<ntd::Vec3f> denoise(
    const std::array<Frame, 7>& frames, const ntd::DenoiseParameters& parameters) {
    const Frame& current = frames[3];
    std::vector<ntd::Vec3f> result;
    result.reserve(static_cast<std::size_t>(kWidth * kHeight));

    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const ntd::PixelGuidance& reference = sampleGuidance(current, x, y);
            ntd::WeightedAccumulator<1> accumulator;
            accumulator.add({sampleBeauty(current, x, y)}, 1.0F);

            for (int dy = -parameters.spatialRadius; dy <= parameters.spatialRadius; ++dy) {
                for (int dx = -parameters.spatialRadius; dx <= parameters.spatialRadius; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const float distanceSquared = static_cast<float>(dx * dx + dy * dy);
                    const float weight = ntd::computeSpatialWeight(reference,
                        sampleGuidance(current, x + dx, y + dy), distanceSquared, parameters);
                    accumulator.add({sampleBeauty(current, x + dx, y + dy)}, weight);
                }
            }

            const bool object = isObject(x, y, 0);
            for (int frameDistance = 1; frameDistance <= parameters.temporalRadius;
                ++frameDistance) {
                for (int direction : {-1, 1}) {
                    const int frameOffset = frameDistance * direction;
                    const Frame& temporal = frames[static_cast<std::size_t>(frameOffset + 3)];
                    const int predictedX = x + (object ? frameOffset : 0);

                    float bestCost = std::numeric_limits<float>::infinity();
                    int bestX = predictedX;
                    int bestY = y;
                    float bestSearchDistanceSquared = 0.0F;
                    bool found = false;

                    for (int dy = -parameters.searchRadius; dy <= parameters.searchRadius; ++dy) {
                        for (int dx = -parameters.searchRadius; dx <= parameters.searchRadius;
                            ++dx) {
                            const ntd::MatchCandidate candidate{
                                {static_cast<float>(dx), static_cast<float>(dy)},
                                sampleGuidance(temporal, predictedX + dx, y + dy),
                            };
                            const float cost =
                                ntd::temporalMatchCost(reference, candidate, parameters);
                            if (cost < bestCost) {
                                bestCost = cost;
                                bestX = predictedX + dx;
                                bestY = y + dy;
                                bestSearchDistanceSquared = static_cast<float>(dx * dx + dy * dy);
                                found = true;
                            }
                        }
                    }

                    if (found) {
                        const ntd::PixelGuidance& matched = sampleGuidance(temporal, bestX, bestY);
                        const float weight = ntd::computeTemporalWeight(reference, matched,
                            bestSearchDistanceSquared, frameDistance, parameters);
                        accumulator.add({sampleBeauty(temporal, bestX, bestY)}, weight);
                    }
                }
            }

            result.push_back(accumulator.value()[0]);
        }
    }

    return result;
}

std::vector<ntd::Vec3f> makeReference() {
    std::vector<ntd::Vec3f> reference;
    reference.reserve(static_cast<std::size_t>(kWidth * kHeight));
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            reference.push_back(referenceColor(isObject(x, y, 0)));
        }
    }
    return reference;
}

float rmse(const std::vector<ntd::Vec3f>& image, const std::vector<ntd::Vec3f>& reference) {
    float squaredError = 0.0F;
    for (std::size_t index = 0; index < image.size(); ++index) {
        squaredError += ntd::squaredDistance(image[index], reference[index]);
    }
    const float sampleCount = static_cast<float>(image.size() * 3U);
    return std::sqrt(squaredError / sampleCount);
}

int displayChannel(float linear) {
    const float encoded = std::pow(std::clamp(linear, 0.0F, 1.0F), 1.0F / 2.2F);
    return static_cast<int>(std::lround(encoded * 255.0F));
}

void writePanel(std::ostream& output, const std::vector<ntd::Vec3f>& pixels, int panelX) {
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const ntd::Vec3f color = pixels[static_cast<std::size_t>(indexOf(x, y))];
            output << "<rect x=\"" << panelX + x * kPanelScale;
            output << "\" y=\"" << 36 + y * kPanelScale;
            output << "\" width=\"" << kPanelScale << "\" height=\"" << kPanelScale;
            output << "\" fill=\"rgb(" << displayChannel(color.x) << ',';
            output << displayChannel(color.y) << ',' << displayChannel(color.z);
            output << ")\"/>\n";
        }
    }
}

void writeSvg(const std::string& path, const std::vector<ntd::Vec3f>& reference,
    const std::vector<ntd::Vec3f>& noisy, const std::vector<ntd::Vec3f>& filtered, float noisyRmse,
    float filteredRmse) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not open output SVG: " + path);
    }

    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"660\" ";
    output << "height=\"190\" viewBox=\"0 0 660 190\">\n";
    output << "<rect width=\"660\" height=\"190\" fill=\"#0d1117\"/>\n";
    output << "<style>text{font-family:ui-monospace,SFMono-Regular,Menlo,";
    output << "monospace;fill:#e6edf3} .small{font-size:11px;fill:#8b949e}";
    output << "</style>\n";
    output << "<text x=\"18\" y=\"21\" font-size=\"14\">";
    output << "Synthetic motion and edge regression</text>\n";

    writePanel(output, reference, 18);
    writePanel(output, noisy, 234);
    writePanel(output, filtered, 450);

    output << "<text x=\"18\" y=\"174\" font-size=\"12\">reference</text>\n";
    output << "<text x=\"234\" y=\"174\" font-size=\"12\">noisy</text>\n";
    output << "<text x=\"450\" y=\"174\" font-size=\"12\">denoised</text>\n";
    output << std::fixed << std::setprecision(4);
    output << "<text x=\"234\" y=\"187\" class=\"small\">RMSE ";
    output << noisyRmse << "</text>\n";
    output << "<text x=\"450\" y=\"187\" class=\"small\">RMSE ";
    output << filteredRmse << "</text>\n";
    output << "</svg>\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outputPath = argc > 1 ? argv[1] : "docs/synthetic-demo.svg";
    std::array<Frame, 7> frames;
    for (int frameOffset = -3; frameOffset <= 3; ++frameOffset) {
        frames[static_cast<std::size_t>(frameOffset + 3)] = makeFrame(frameOffset);
    }

    const std::vector<ntd::Vec3f> reference = makeReference();
    const std::vector<ntd::Vec3f>& noisy = frames[3].beauty;
    const std::vector<ntd::Vec3f> filtered = denoise(frames, demoParameters());
    const float noisyError = rmse(noisy, reference);
    const float filteredError = rmse(filtered, reference);

    if (!(filteredError < noisyError)) {
        std::cerr << "Synthetic denoise did not improve RMSE\n";
        return EXIT_FAILURE;
    }

    try {
        writeSvg(outputPath, reference, noisy, filtered, noisyError, filteredError);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "noisy RMSE:    " << noisyError << '\n';
    std::cout << "denoised RMSE: " << filteredError << '\n';
    std::cout << "wrote: " << outputPath << '\n';
    return EXIT_SUCCESS;
}
