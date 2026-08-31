#pragma once

#include <math/utils.hpp>
#include <math/vector.hpp>

#include <cstddef>

namespace math {

template <typename T, std::size_t N>
struct box {
    vector<T, N> min;
    vector<T, N> max;

    friend constexpr bool operator==(box const &, box const &) = default;
};

template <typename T, std::size_t N>
constexpr vector<T, N> diagonal(box<T, N> const &b) {
    return b.max - b.min;
}

template <typename T, std::size_t N>
constexpr vector<T, N> center(box<T, N> const &b) {
    return (b.min + b.max) / T{2};
}

template <typename T, std::size_t N>
constexpr bool empty(box<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        if (b.max[i] <= b.min[i]) {
            return true;
        }
    }
    return false;
}

template <typename T, std::size_t N>
constexpr bool contains(box<T, N> const &b, vector<T, N> const &p) {
    for (std::size_t i = 0; i < N; ++i) {
        if (p[i] < b.min[i] || p[i] > b.max[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, std::size_t N>
constexpr box<T, N> &operator&=(box<T, N> &a, box<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        make_max(a.min[i], b.min[i]);
        make_min(a.max[i], b.max[i]);
    }
    return a;
}

template <typename T, std::size_t N>
constexpr box<T, N> &operator|=(box<T, N> &a, box<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        make_min(a.min[i], b.min[i]);
        make_max(a.max[i], b.max[i]);
    }
    return a;
}

template <typename T, std::size_t N>
constexpr box<T, N> &operator|=(box<T, N> &a, vector<T, N> const &p) {
    for (std::size_t i = 0; i < N; ++i) {
        make_min(a.min[i], p[i]);
        make_max(a.max[i], p[i]);
    }
    return a;
}

template <typename T, std::size_t N>
constexpr box<T, N> operator&(box<T, N> a, box<T, N> const &b) {
    return a &= b;
}

template <typename T, std::size_t N>
constexpr box<T, N> operator|(box<T, N> a, box<T, N> const &b) {
    return a |= b;
}

template <typename T, std::size_t N>
constexpr box<T, N> operator|(box<T, N> a, vector<T, N> const &p) {
    return a |= p;
}

template <typename T, std::size_t N>
constexpr box<T, N> operator|(vector<T, N> const &p, box<T, N> a) {
    return a |= p;
}

}  // namespace math
