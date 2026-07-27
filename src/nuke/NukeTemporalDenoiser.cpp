#include "ntd/DenoiseParameters.h"
#include "ntd/GuidedWeight.h"
#include "ntd/TemporalMatcher.h"
#include "ntd/WeightedAccumulator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "DDImage/Iop.h"
#include "DDImage/Knobs.h"
#include "DDImage/Row.h"
#include "DDImage/Tile.h"

using namespace DD::Image;

namespace {

constexpr int kContextCount = 7;
constexpr int kFilteredPassCount = 5;

struct LocatedMatch {
    int x = 0;
    int y = 0;
    float searchDistanceSquared = 0.0F;
    ntd::PixelGuidance guidance;
    bool found = false;
};

struct OutputTarget {
    Channel channel = Chan_Black;
    float* destination = nullptr;
    int filteredPass = -1;
    int component = 0;
};

}  // namespace

class GinzburgTemporalDenoiser final : public Iop {
public:
    explicit GinzburgTemporalDenoiser(Node* node) : Iop(node) {
        beauty_[0] = Chan_Red;
        beauty_[1] = Chan_Green;
        beauty_[2] = Chan_Blue;
        depth_[0] = Chan_Z;
    }

    void _validate(bool forReal) override;
    void _request(int x, int y, int r, int t, ChannelMask channels, int count) override;
    void engine(int y, int x, int r, ChannelMask channels, Row& outputRow) override;
    const OutputContext& inputContext(
        int inputIndex, int contextIndex, OutputContext& context) const override;

    int maximum_inputs() const override { return 1; }
    int minimum_inputs() const override { return 1; }
    int split_input(int /*inputIndex*/) const override { return kContextCount; }

    void knobs(Knob_Callback callback) override;
    const char* Class() const override { return kClassName; }
    const char* node_help() const override { return kHelp; }

private:
    static Iop* create(Node* node) { return new GinzburgTemporalDenoiser(node); }

    ntd::DenoiseParameters parameters() const noexcept;
    void addRequiredChannels(ChannelSet& channels) const;
    ntd::Vec3f readVector(Tile& tile, const Channel* channels, int x, int y) const;
    ntd::PixelGuidance readGuidance(Tile& tile, int x, int y) const;
    std::array<ntd::Vec3f, kFilteredPassCount> readPasses(Tile& tile, int x, int y) const;
    ntd::Vec2f readMotion(Tile& tile, int x, int y, const ntd::DenoiseParameters& params) const;
    ntd::Vec2f predictPosition(const std::array<std::unique_ptr<Tile>, kContextCount>& tiles, int x,
        int y, int frameOffset, const ntd::DenoiseParameters& params) const;
    LocatedMatch findMatch(Tile& tile, const ntd::PixelGuidance& reference,
        const ntd::Vec2f& predicted, const ntd::DenoiseParameters& params) const;
    int filteredPass(Channel channel, int& component) const;

    int spatialRadius_ = 5;
    int searchRadius_ = 3;
    int temporalRadius_ = 3;
    float spatialWeight_ = 0.7F;
    float temporalWeight_ = 0.3F;
    float frameDecay_ = 0.35F;
    float motionScale_ = 1.0F;
    float maximumMotionPerFrame_ = 32.0F;
    float beautySigma_ = 0.2F;
    float albedoSigma_ = 0.1F;
    float normalSigma_ = 0.2F;
    float depthSigma_ = 1.0F;
    float positionSigma_ = 1.0F;
    float spatialSigma_ = 3.0F;
    float matchBeautyThreshold_ = 0.5F;
    float matchAlbedoThreshold_ = 0.25F;
    float matchPositionThreshold_ = 2.0F;
    bool useMotionVectors_ = true;

    Channel motion_[2] = {Chan_Black, Chan_Black};
    Channel depth_[1] = {Chan_Black};
    Channel position_[3] = {Chan_Black, Chan_Black, Chan_Black};
    Channel beauty_[3] = {Chan_Black, Chan_Black, Chan_Black};
    Channel normal_[3] = {Chan_Black, Chan_Black, Chan_Black};
    Channel albedo_[3] = {Chan_Black, Chan_Black, Chan_Black};
    Channel extra_[4][3] = {
        {Chan_Black, Chan_Black, Chan_Black},
        {Chan_Black, Chan_Black, Chan_Black},
        {Chan_Black, Chan_Black, Chan_Black},
        {Chan_Black, Chan_Black, Chan_Black},
    };

    static const Description description_;
    static const char* const kClassName;
    static const char* const kHelp;
};

const Iop::Description GinzburgTemporalDenoiser::description_(
    kClassName, "Filter/GinzburgTemporalDenoiser", GinzburgTemporalDenoiser::create);

const char* const GinzburgTemporalDenoiser::kClassName = "GinzburgTemporalDenoiser";
const char* const GinzburgTemporalDenoiser::kHelp =
    "AOV-guided spatiotemporal denoiser. It searches motion-compensated "
    "neighbors across three frames on either side, rejects disocclusions "
    "with beauty, albedo, and position guides, then applies a geometry-aware "
    "spatial filter to beauty and four optional RGB passes.";

ntd::DenoiseParameters GinzburgTemporalDenoiser::parameters() const noexcept {
    ntd::DenoiseParameters params;
    params.spatialRadius = spatialRadius_;
    params.searchRadius = searchRadius_;
    params.temporalRadius = temporalRadius_;
    params.spatialWeight = spatialWeight_;
    params.temporalWeight = temporalWeight_;
    params.frameDecay = frameDecay_;
    params.motionScale = motionScale_;
    params.maximumMotionPerFrame = maximumMotionPerFrame_;
    params.beautySigma = beautySigma_;
    params.albedoSigma = albedoSigma_;
    params.normalSigma = normalSigma_;
    params.depthSigma = depthSigma_;
    params.positionSigma = positionSigma_;
    params.spatialSigma = spatialSigma_;
    params.matchBeautyThreshold = matchBeautyThreshold_;
    params.matchAlbedoThreshold = matchAlbedoThreshold_;
    params.matchPositionThreshold = matchPositionThreshold_;
    params.sanitize();
    return params;
}

void GinzburgTemporalDenoiser::knobs(Knob_Callback callback) {
    Int_knob(callback, &spatialRadius_, "spatial_radius", "spatial radius");
    SetRange(callback, 0, ntd::DenoiseParameters::kMaximumRadius);
    Tooltip(callback, "Radius of the current-frame guided filter.");
    Int_knob(callback, &searchRadius_, "search_radius", "temporal search radius");
    SetRange(callback, 0, ntd::DenoiseParameters::kMaximumRadius);
    Tooltip(callback, "Local search radius around the motion prediction.");
    Int_knob(callback, &temporalRadius_, "temporal_radius", "frames each side");
    SetRange(callback, 0, ntd::DenoiseParameters::kMaximumTemporalRadius);
    Tooltip(callback, "Number of frames sampled on each side. Clamped to 0-3.");

    Float_knob(callback, &spatialWeight_, "spatial_weight", "spatial weight");
    SetRange(callback, 0.0, 2.0);
    Tooltip(callback, "Contribution of current-frame guided neighbors.");
    Float_knob(callback, &temporalWeight_, "temporal_weight", "temporal weight");
    SetRange(callback, 0.0, 2.0);
    Tooltip(callback, "Contribution of accepted temporal correspondences.");
    Float_knob(callback, &frameDecay_, "frame_decay", "frame decay");
    SetRange(callback, 0.0, 4.0);
    Tooltip(callback, "Exponential attenuation applied to older frames.");

    Bool_knob(callback, &useMotionVectors_, "use_motion", "use motion vectors");
    Float_knob(callback, &motionScale_, "motion_scale", "motion scale");
    Tooltip(callback, "Scale applied to the selected XY motion channels.");
    Float_knob(callback, &maximumMotionPerFrame_, "maximum_motion", "maximum motion / frame");
    SetRange(callback, 0.0, 4096.0);
    Tooltip(callback, "Conservative motion bound used to request input tiles.");

    Float_knob(callback, &beautySigma_, "beauty_sigma", "beauty sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100.0);
    Float_knob(callback, &albedoSigma_, "albedo_sigma", "albedo sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100.0);
    Float_knob(callback, &normalSigma_, "normal_sigma", "normal sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100.0);
    Float_knob(callback, &depthSigma_, "depth_sigma", "depth sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100000.0);
    Float_knob(callback, &positionSigma_, "position_sigma", "position sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100000.0);
    Float_knob(callback, &spatialSigma_, "spatial_sigma", "spatial sigma");
    SetRange(callback, ntd::DenoiseParameters::kMinimumSigma, 100.0);
    Tooltip(callback, "Positive guide sigmas. Invalid values are made safe.");

    Float_knob(
        callback, &matchBeautyThreshold_, "match_beauty_threshold", "match beauty threshold");
    SetRange(callback, 0.0, 100.0);
    Float_knob(
        callback, &matchAlbedoThreshold_, "match_albedo_threshold", "match albedo threshold");
    SetRange(callback, 0.0, 100.0);
    Float_knob(
        callback, &matchPositionThreshold_, "match_position_threshold", "match position threshold");
    SetRange(callback, 0.0, 100000.0);
    Tooltip(callback, "Hard temporal rejection thresholds.");

    Input_Channel_knob(callback, motion_, 2, 0, "motion", "motion XY");
    Tooltip(callback, "Forward pixel-space motion vector channels.");
    Input_Channel_knob(callback, position_, 3, 0, "position", "position RGB");
    Input_Channel_knob(callback, beauty_, 3, 0, "beauty", "beauty RGB");
    Input_Channel_knob(callback, depth_, 1, 0, "depth", "depth");
    Input_Channel_knob(callback, normal_, 3, 0, "normal", "normal RGB");
    Input_Channel_knob(callback, albedo_, 3, 0, "albedo", "albedo RGB");
    Input_Channel_knob(callback, extra_[0], 3, 0, "extra_0", "extra RGB 0");
    Input_Channel_knob(callback, extra_[1], 3, 0, "extra_1", "extra RGB 1");
    Input_Channel_knob(callback, extra_[2], 3, 0, "extra_2", "extra RGB 2");
    Input_Channel_knob(callback, extra_[3], 3, 0, "extra_3", "extra RGB 3");
}

void GinzburgTemporalDenoiser::_validate(bool /*forReal*/) {
    copy_info(0);
    info_.pad(parameters().requestPadding());
}

const OutputContext& GinzburgTemporalDenoiser::inputContext(
    int /*inputIndex*/, int contextIndex, OutputContext& context) const {
    static constexpr std::array<int, kContextCount> kFrameOffsets{0, 1, 2, 3, -1, -2, -3};
    context = outputContext();
    if (contextIndex >= 0 && contextIndex < kContextCount) {
        context.setFrame(context.frame() + kFrameOffsets[contextIndex]);
    }
    return context;
}

void GinzburgTemporalDenoiser::addRequiredChannels(ChannelSet& channels) const {
    for (Channel channel : motion_) {
        channels += channel;
    }
    for (Channel channel : position_) {
        channels += channel;
    }
    for (Channel channel : beauty_) {
        channels += channel;
    }
    channels += depth_[0];
    for (Channel channel : normal_) {
        channels += channel;
    }
    for (Channel channel : albedo_) {
        channels += channel;
    }
    for (const auto& pass : extra_) {
        for (Channel channel : pass) {
            channels += channel;
        }
    }
}

void GinzburgTemporalDenoiser::_request(
    int x, int y, int r, int t, ChannelMask channels, int count) {
    const ntd::DenoiseParameters params = parameters();
    ChannelSet required(channels);
    addRequiredChannels(required);

    const int padding = params.requestPadding();
    input(0, 0)->request(x - padding, y - padding, r + padding, t + padding, required, count);
    for (int distance = 1; distance <= params.temporalRadius; ++distance) {
        const std::array<int, 2> contexts{distance, 3 + distance};
        for (int context : contexts) {
            input(0, context)
                ->request(x - padding, y - padding, r + padding, t + padding, required, count);
        }
    }
}

ntd::Vec3f GinzburgTemporalDenoiser::readVector(
    Tile& tile, const Channel* channels, int x, int y) const {
    const int safeX = tile.clampx(x);
    const int safeY = tile.clampy(y);
    return {
        tile[channels[0]][safeY][safeX],
        tile[channels[1]][safeY][safeX],
        tile[channels[2]][safeY][safeX],
    };
}

ntd::PixelGuidance GinzburgTemporalDenoiser::readGuidance(Tile& tile, int x, int y) const {
    const int safeX = tile.clampx(x);
    const int safeY = tile.clampy(y);
    return {
        readVector(tile, beauty_, safeX, safeY),
        readVector(tile, albedo_, safeX, safeY),
        readVector(tile, normal_, safeX, safeY),
        readVector(tile, position_, safeX, safeY),
        tile[depth_[0]][safeY][safeX],
    };
}

std::array<ntd::Vec3f, kFilteredPassCount> GinzburgTemporalDenoiser::readPasses(
    Tile& tile, int x, int y) const {
    return {
        readVector(tile, beauty_, x, y),
        readVector(tile, extra_[0], x, y),
        readVector(tile, extra_[1], x, y),
        readVector(tile, extra_[2], x, y),
        readVector(tile, extra_[3], x, y),
    };
}

ntd::Vec2f GinzburgTemporalDenoiser::readMotion(
    Tile& tile, int x, int y, const ntd::DenoiseParameters& params) const {
    if (!useMotionVectors_) {
        return {};
    }

    const int safeX = tile.clampx(x);
    const int safeY = tile.clampy(y);
    const float limit = params.maximumMotionPerFrame;
    return {
        std::clamp(tile[motion_[0]][safeY][safeX] * params.motionScale, -limit, limit),
        std::clamp(tile[motion_[1]][safeY][safeX] * params.motionScale, -limit, limit),
    };
}

ntd::Vec2f GinzburgTemporalDenoiser::predictPosition(
    const std::array<std::unique_ptr<Tile>, kContextCount>& tiles, int x, int y, int frameOffset,
    const ntd::DenoiseParameters& params) const {
    ntd::Vec2f predicted{static_cast<float>(x), static_cast<float>(y)};
    const int frameDistance = std::abs(frameOffset);

    if (frameOffset > 0) {
        for (int step = 0; step < frameDistance; ++step) {
            const ntd::Vec2f motion =
                readMotion(*tiles[step], static_cast<int>(std::lround(predicted.x)),
                    static_cast<int>(std::lround(predicted.y)), params);
            predicted = predicted + motion;
        }
    } else {
        for (int step = 1; step <= frameDistance; ++step) {
            const int context = 3 + step;
            const ntd::Vec2f motion =
                readMotion(*tiles[context], static_cast<int>(std::lround(predicted.x)),
                    static_cast<int>(std::lround(predicted.y)), params);
            predicted = predicted + motion * -1.0F;
        }
    }

    return predicted;
}

LocatedMatch GinzburgTemporalDenoiser::findMatch(Tile& tile, const ntd::PixelGuidance& reference,
    const ntd::Vec2f& predicted, const ntd::DenoiseParameters& params) const {
    LocatedMatch best;
    float bestCost = std::numeric_limits<float>::infinity();

    for (int dy = -params.searchRadius; dy <= params.searchRadius; ++dy) {
        for (int dx = -params.searchRadius; dx <= params.searchRadius; ++dx) {
            const int candidateX = static_cast<int>(std::lround(predicted.x)) + dx;
            const int candidateY = static_cast<int>(std::lround(predicted.y)) + dy;
            const ntd::PixelGuidance candidate = readGuidance(tile, candidateX, candidateY);
            const ntd::MatchCandidate matchCandidate{
                {static_cast<float>(dx), static_cast<float>(dy)}, candidate};
            const float cost = ntd::temporalMatchCost(reference, matchCandidate, params);

            if (cost < bestCost) {
                best.x = candidateX;
                best.y = candidateY;
                best.searchDistanceSquared = static_cast<float>(dx * dx + dy * dy);
                best.guidance = candidate;
                best.found = true;
                bestCost = cost;
            }
        }
    }

    return best;
}

int GinzburgTemporalDenoiser::filteredPass(Channel channel, int& component) const {
    for (int index = 0; index < 3; ++index) {
        if (channel == beauty_[index]) {
            component = index;
            return 0;
        }
    }

    for (int pass = 0; pass < 4; ++pass) {
        for (int index = 0; index < 3; ++index) {
            if (channel == extra_[pass][index]) {
                component = index;
                return pass + 1;
            }
        }
    }

    return -1;
}

void GinzburgTemporalDenoiser::engine(int y, int x, int r, ChannelMask channels, Row& outputRow) {
    if (aborted()) {
        return;
    }

    const ntd::DenoiseParameters params = parameters();
    ChannelSet required(channels);
    addRequiredChannels(required);
    const int padding = params.requestPadding();

    std::array<std::unique_ptr<Tile>, kContextCount> tiles;
    tiles[0] = std::make_unique<Tile>(
        *input(0, 0), x - padding, y - padding, r + padding, y + padding + 1, required);
    for (int distance = 1; distance <= params.temporalRadius; ++distance) {
        const std::array<int, 2> contexts{distance, 3 + distance};
        for (int context : contexts) {
            tiles[context] = std::make_unique<Tile>(*input(0, context), x - padding, y - padding,
                r + padding, y + padding + 1, required);
        }
    }

    std::vector<OutputTarget> outputTargets;
    foreach (channel, channels) {
        int component = 0;
        outputTargets.push_back(
            {channel, outputRow.writable(channel), filteredPass(channel, component), component});
    }

    for (int pixelX = x; pixelX < r; ++pixelX) {
        const ntd::PixelGuidance reference = readGuidance(*tiles[0], pixelX, y);
        ntd::WeightedAccumulator<kFilteredPassCount> accumulator;
        accumulator.add(readPasses(*tiles[0], pixelX, y), 1.0F);

        for (int dy = -params.spatialRadius; dy <= params.spatialRadius; ++dy) {
            for (int dx = -params.spatialRadius; dx <= params.spatialRadius; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const ntd::PixelGuidance candidate = readGuidance(*tiles[0], pixelX + dx, y + dy);
                const float pixelDistanceSquared = static_cast<float>(dx * dx + dy * dy);
                const float weight =
                    ntd::computeSpatialWeight(reference, candidate, pixelDistanceSquared, params);
                accumulator.add(readPasses(*tiles[0], pixelX + dx, y + dy), weight);
            }
        }

        for (int frameDistance = 1; frameDistance <= params.temporalRadius; ++frameDistance) {
            const std::array<int, 2> offsets{frameDistance, -frameDistance};
            for (int frameOffset : offsets) {
                const int context = frameOffset > 0 ? frameDistance : 3 + frameDistance;
                const ntd::Vec2f predicted = predictPosition(tiles, pixelX, y, frameOffset, params);

#if 0
                // Direct frame merge for shots with reliable motion vectors.
                // This skips the correspondence search and samples the
                // motion-compensated coordinate directly.
                const int mergeX = static_cast<int>(std::lround(predicted.x));
                const int mergeY = static_cast<int>(std::lround(predicted.y));
                const ntd::PixelGuidance mergeGuidance =
                    readGuidance(*tiles[context], mergeX, mergeY);
                const float mergeWeight = ntd::computeTemporalWeight(
                    reference, mergeGuidance, 0.0F, frameDistance, params);
                accumulator.add(
                    readPasses(*tiles[context], mergeX, mergeY), mergeWeight);
                continue;
#endif

                const LocatedMatch match = findMatch(*tiles[context], reference, predicted, params);
                if (!match.found) {
                    continue;
                }

                const float weight = ntd::computeTemporalWeight(
                    reference, match.guidance, match.searchDistanceSquared, frameDistance, params);
                accumulator.add(readPasses(*tiles[context], match.x, match.y), weight);
            }
        }

        const auto result = accumulator.value();
        for (const OutputTarget& target : outputTargets) {
            float value = 0.0F;
            if (target.filteredPass >= 0) {
                const ntd::Vec3f& filtered = result[static_cast<std::size_t>(target.filteredPass)];
                if (target.component == 0) {
                    value = filtered.x;
                } else if (target.component == 1) {
                    value = filtered.y;
                } else {
                    value = filtered.z;
                }
            } else {
                const int safeX = tiles[0]->clampx(pixelX);
                const int safeY = tiles[0]->clampy(y);
                value = (*tiles[0])[target.channel][safeY][safeX];
            }
            target.destination[pixelX] = value;
        }
    }
}
