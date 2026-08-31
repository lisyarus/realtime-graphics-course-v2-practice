#pragma once

#include <cmath>
#include <cstddef>
#include <type_traits>

namespace math {

template <typename T, std::size_t N>
struct vector {
    T data[N];

    constexpr vector() = default;

    template <typename... Args>
        requires (sizeof...(Args) == N && (std::is_convertible_v<Args, T> && ...))
    constexpr vector(Args... args)
        : data{static_cast<T>(args)...} {}

    constexpr T &operator[](std::size_t i) { return data[i]; }
    constexpr T const &operator[](std::size_t i) const { return data[i]; }

    constexpr T &x() requires (N >= 1) { return data[0]; }
    constexpr T const &x() const requires (N >= 1) { return data[0]; }
    constexpr T &y() requires (N >= 2) { return data[1]; }
    constexpr T const &y() const requires (N >= 2) { return data[1]; }
    constexpr T &z() requires (N >= 3) { return data[2]; }
    constexpr T const &z() const requires (N >= 3) { return data[2]; }
    constexpr T &w() requires (N >= 4) { return data[3]; }
    constexpr T const &w() const requires (N >= 4) { return data[3]; }

    friend constexpr bool operator==(vector const &, vector const &) = default;
};

template <typename T, std::size_t N>
constexpr vector<T, N> &operator+=(vector<T, N> &a, vector<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] += b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> &operator-=(vector<T, N> &a, vector<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] -= b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> &operator*=(vector<T, N> &a, T s) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] *= s;
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> &operator*=(vector<T, N> &a, vector<T, N> const &b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] *= b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> &operator/=(vector<T, N> &a, T s) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] /= s;
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator+(vector<T, N> a, vector<T, N> const &b) {
    return a += b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator-(vector<T, N> a, vector<T, N> const &b) {
    return a -= b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator-(vector<T, N> v) {
    for (std::size_t i = 0; i < N; ++i) {
        v[i] = -v[i];
    }
    return v;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator*(vector<T, N> v, T s) {
    return v *= s;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator*(T s, vector<T, N> v) {
    return v *= s;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator*(vector<T, N> a, vector<T, N> const &b) {
    return a *= b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator/(vector<T, N> v, T s) {
    return v /= s;
}

template <typename T, std::size_t N>
constexpr T dot(vector<T, N> const &a, vector<T, N> const &b) {
    T sum{};
    for (std::size_t i = 0; i < N; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

template <typename T, std::size_t N>
constexpr T length_squared(vector<T, N> const &v) {
    return dot(v, v);
}

template <typename T, std::size_t N>
auto length(vector<T, N> const &v) {
    using std::sqrt;
    return sqrt(length_squared(v));
}

template <typename T, std::size_t N>
constexpr T length_max(vector<T, N> const &v) {
    using std::abs;
    T result{};
    for (std::size_t i = 0; i < N; ++i) {
        T const a = abs(v[i]);
        if (a > result) {
            result = a;
        }
    }
    return result;
}

template <typename T, std::size_t N>
vector<T, N> normalized(vector<T, N> const &v) {
    return v / static_cast<T>(length(v));
}

template <typename T, std::size_t N>
constexpr vector<T, N> lerp(vector<T, N> const &a, vector<T, N> const &b, T t) {
    return a * (T{1} - t) + b * t;
}

template <typename T>
constexpr vector<T, 3> cross(vector<T, 3> const &a, vector<T, 3> const &b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    };
}

}  // namespace math
