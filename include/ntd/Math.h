#pragma once

#include <cmath>

namespace ntd {

struct Vec2f {
    float x = 0.0F;
    float y = 0.0F;
};

struct Vec3f {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;

    Vec3f& operator+=(const Vec3f& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }
};

[[nodiscard]] inline Vec2f operator+(Vec2f lhs, const Vec2f& rhs) noexcept {
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

[[nodiscard]] inline Vec2f operator*(Vec2f value, float scale) noexcept {
    value.x *= scale;
    value.y *= scale;
    return value;
}

[[nodiscard]] inline Vec3f operator*(Vec3f value, float scale) noexcept {
    value.x *= scale;
    value.y *= scale;
    value.z *= scale;
    return value;
}

[[nodiscard]] inline Vec3f operator/(Vec3f value, float divisor) noexcept {
    value.x /= divisor;
    value.y /= divisor;
    value.z /= divisor;
    return value;
}

[[nodiscard]] inline float squaredLength(const Vec2f& value) noexcept {
    return value.x * value.x + value.y * value.y;
}

[[nodiscard]] inline float squaredDistance(const Vec3f& lhs, const Vec3f& rhs) noexcept {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    const float dz = lhs.z - rhs.z;
    return dx * dx + dy * dy + dz * dz;
}

[[nodiscard]] inline bool isFinite(const Vec2f& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] inline bool isFinite(const Vec3f& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

}  // namespace ntd
