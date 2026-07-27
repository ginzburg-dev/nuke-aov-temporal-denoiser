#pragma once

#include "ntd/Math.h"

#include <array>
#include <cstddef>

namespace ntd {

template <std::size_t PassCount>
class WeightedAccumulator {
public:
    void add(const std::array<Vec3f, PassCount>& sample, float weight) noexcept {
        if (!(weight > 0.0F)) {
            return;
        }

        for (std::size_t pass = 0; pass < PassCount; ++pass) {
            weightedSum_[pass] += sample[pass] * weight;
        }
        totalWeight_ += weight;
    }

    [[nodiscard]] std::array<Vec3f, PassCount> value() const noexcept {
        if (!(totalWeight_ > 0.0F)) {
            return {};
        }

        std::array<Vec3f, PassCount> result{};
        for (std::size_t pass = 0; pass < PassCount; ++pass) {
            result[pass] = weightedSum_[pass] / totalWeight_;
        }
        return result;
    }

private:
    std::array<Vec3f, PassCount> weightedSum_{};
    float totalWeight_ = 0.0F;
};

}  // namespace ntd
