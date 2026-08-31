#pragma once

namespace math {

template <typename T>
constexpr bool make_min(T &a, T b) {
    if (b < a) {
        a = b;
        return true;
    }
    return false;
}

template <typename T>
constexpr bool make_max(T &a, T b) {
    if (b > a) {
        a = b;
        return true;
    }
    return false;
}

template <typename T>
constexpr T clamp(T x, T low, T high) {
    make_max(x, low);
    make_min(x, high);
    return x;
}

template <typename T>
constexpr T smoothstep(T edge0, T edge1, T x) {
    T const t = clamp((x - edge0) / (edge1 - edge0), T{0}, T{1});
    return t * t * (T{3} - T{2} * t);
}

}  // namespace math
