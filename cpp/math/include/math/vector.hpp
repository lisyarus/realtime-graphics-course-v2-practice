#pragma once

#include <math/utils.hpp>

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

    constexpr T & operator [] (std::size_t i) { return data[i]; }
    constexpr T const & operator [] (std::size_t i) const { return data[i]; }

    constexpr T & x() requires (N >= 1) { return data[0]; }
    constexpr T const & x() const requires (N >= 1) { return data[0]; }
    constexpr T & y() requires (N >= 2) { return data[1]; }
    constexpr T const & y() const requires (N >= 2) { return data[1]; }
    constexpr T & z() requires (N >= 3) { return data[2]; }
    constexpr T const & z() const requires (N >= 3) { return data[2]; }
    constexpr T & w() requires (N >= 4) { return data[3]; }
    constexpr T const & w() const requires (N >= 4) { return data[3]; }

    static constexpr vector zero() {
        return vector{};
    }

    friend constexpr bool operator == (vector const &, vector const &) = default;
};

template <typename ... Args>
vector(Args...) -> vector<std::common_type_t<Args...>, sizeof...(Args)>;

template <std::size_t M, typename T, std::size_t N>
    requires (M <= N)
constexpr vector<T, M> head(vector<T, N> const & v) {
    vector<T, M> r;
    for (std::size_t i = 0; i < M; ++i)
        r[i] = v[i];
    return r;
}

template <typename T, std::size_t N>
constexpr vector<T, N + 1> append_zero(vector<T, N> const & v) {
    vector<T, N + 1> r;
    for (std::size_t i = 0; i < N; ++i)
        r[i] = v[i];
    r[N] = T{0};
    return r;
}

template <typename T, std::size_t N>
constexpr vector<T, N + 1> append_one(vector<T, N> const & v) {
    vector<T, N + 1> r;
    for (std::size_t i = 0; i < N; ++i)
        r[i] = v[i];
    r[N] = T{1};
    return r;
}

template <typename H, typename T, std::size_t N>
constexpr vector<H, N> cast(vector<T, N> const & a) {
    vector<H, N> result;
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = static_cast<H>(a[i]);
    }
    return result;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator += (vector<T, N> & a, vector<T, N> const & b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] += b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator -= (vector<T, N> & a, vector<T, N> const & b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] -= b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator *= (vector<T, N> & a, T s) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] *= s;
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator *= (vector<T, N> & a, vector<T, N> const & b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] *= b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator /= (vector<T, N> & a, T s) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] /= s;
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> & operator /= (vector<T, N> & a, vector<T, N> const & b) {
    for (std::size_t i = 0; i < N; ++i) {
        a[i] /= b[i];
    }
    return a;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator + (vector<T, N> a, vector<T, N> const & b) {
    return a += b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator - (vector<T, N> a, vector<T, N> const & b) {
    return a -= b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator - (vector<T, N> v) {
    for (std::size_t i = 0; i < N; ++i) {
        v[i] = -v[i];
    }
    return v;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator * (vector<T, N> v, T s) {
    return v *= s;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator * (T s, vector<T, N> v) {
    return v *= s;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator * (vector<T, N> a, vector<T, N> const & b) {
    return a *= b;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator / (vector<T, N> v, T s) {
    return v /= s;
}

template <typename T, std::size_t N>
constexpr vector<T, N> operator / (vector<T, N> a, vector<T, N> const & b) {
    return a /= b;
}

template <typename T, std::size_t N>
constexpr T dot(vector<T, N> const & a, vector<T, N> const & b) {
    T sum{};
    for (std::size_t i = 0; i < N; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

template <typename T, std::size_t N>
constexpr T length_squared(vector<T, N> const & v) {
    return dot(v, v);
}

template <typename T, std::size_t N>
auto length(vector<T, N> const & v) {
    using std::sqrt;
    return sqrt(length_squared(v));
}

template <typename T, std::size_t N>
constexpr T length_max(vector<T, N> const & v) {
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
vector<T, N> normalized(vector<T, N> const & v) {
    return v / static_cast<T>(length(v));
}

template <typename T>
constexpr vector<T, 3> cross(vector<T, 3> const & a, vector<T, 3> const & b) {
    return {
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    };
}

// v0 and v1 are assumed to be normalized
template <typename T, std::size_t N>
constexpr vector<T, N> slerp(vector<T, N> const & v0, vector<T, N> const & v1, T const & t)
{
    using std::sin;
    using std::acos;

    auto const d = dot(v0, v1);

    // Prevent division by zero
    if (d >= T{1})
        return lerp(v0, v1, t);

    auto const angle = acos(d);

    // NB: the case of omega ~ pi is ambiguous and isn't handled in any special way
    auto const s = sin(angle);
    auto const w0 = std::sin((1 - t) * angle) / s;
    auto const w1 = (sin(t * angle) / s);
    return w0 * v0 + w1 * v1;
}

}  // namespace math
