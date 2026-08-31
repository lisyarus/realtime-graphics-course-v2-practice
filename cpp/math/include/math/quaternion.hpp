#pragma once

#include <math/vector.hpp>

#include <cmath>

namespace math {

template <typename T>
struct quaternion {
    vector<T, 4> data;

    constexpr T &x() { return data.x(); }
    constexpr T const &x() const { return data.x(); }
    constexpr T &y() { return data.y(); }
    constexpr T const &y() const { return data.y(); }
    constexpr T &z() { return data.z(); }
    constexpr T const &z() const { return data.z(); }
    constexpr T &w() { return data.w(); }
    constexpr T const &w() const { return data.w(); }

    static constexpr quaternion identity() {
        return {{T{0}, T{0}, T{0}, T{1}}};
    }

    static quaternion axis_angle(vector<T, 3> const &axis, T angle) {
        using std::cos;
        using std::sin;
        T const half = angle / T{2};
        T const s = static_cast<T>(sin(half));
        return {{axis.x() * s, axis.y() * s, axis.z() * s, static_cast<T>(cos(half))}};
    }

    friend constexpr bool operator==(quaternion const &, quaternion const &) = default;
};

template <typename T>
constexpr quaternion<T> &operator+=(quaternion<T> &a, quaternion<T> const &b) {
    a.data += b.data;
    return a;
}

template <typename T>
constexpr quaternion<T> &operator-=(quaternion<T> &a, quaternion<T> const &b) {
    a.data -= b.data;
    return a;
}

template <typename T>
constexpr quaternion<T> &operator*=(quaternion<T> &a, T s) {
    a.data *= s;
    return a;
}

template <typename T>
constexpr quaternion<T> &operator/=(quaternion<T> &a, T s) {
    a.data /= s;
    return a;
}

template <typename T>
constexpr quaternion<T> &operator*=(quaternion<T> &a, quaternion<T> const &b) {
    quaternion<T> const q = a;
    a.x() = q.w() * b.x() + q.x() * b.w() + q.y() * b.z() - q.z() * b.y();
    a.y() = q.w() * b.y() - q.x() * b.z() + q.y() * b.w() + q.z() * b.x();
    a.z() = q.w() * b.z() + q.x() * b.y() - q.y() * b.x() + q.z() * b.w();
    a.w() = q.w() * b.w() - q.x() * b.x() - q.y() * b.y() - q.z() * b.z();
    return a;
}

template <typename T>
constexpr quaternion<T> operator+(quaternion<T> a, quaternion<T> const &b) {
    return a += b;
}

template <typename T>
constexpr quaternion<T> operator-(quaternion<T> a, quaternion<T> const &b) {
    return a -= b;
}

template <typename T>
constexpr quaternion<T> operator-(quaternion<T> q) {
    q.data = -q.data;
    return q;
}

template <typename T>
constexpr quaternion<T> operator*(quaternion<T> q, T s) {
    return q *= s;
}

template <typename T>
constexpr quaternion<T> operator*(T s, quaternion<T> q) {
    return q *= s;
}

template <typename T>
constexpr quaternion<T> operator/(quaternion<T> q, T s) {
    return q /= s;
}

template <typename T>
constexpr quaternion<T> operator*(quaternion<T> a, quaternion<T> const &b) {
    return a *= b;
}

template <typename T>
constexpr T dot(quaternion<T> const &a, quaternion<T> const &b) {
    return dot(a.data, b.data);
}

template <typename T>
constexpr T length_squared(quaternion<T> const &q) {
    return length_squared(q.data);
}

template <typename T>
auto length(quaternion<T> const &q) {
    return length(q.data);
}

template <typename T>
quaternion<T> normalized(quaternion<T> const &q) {
    return {normalized(q.data)};
}

template <typename T>
constexpr quaternion<T> conjugate(quaternion<T> q) {
    q.x() = -q.x();
    q.y() = -q.y();
    q.z() = -q.z();
    return q;
}

template <typename T>
constexpr quaternion<T> inverse(quaternion<T> const &q) {
    return conjugate(q) / length_squared(q);
}

template <typename T>
constexpr quaternion<T> lerp(quaternion<T> const &a, quaternion<T> const &b, T t) {
    return {lerp(a.data, b.data, t)};
}

template <typename T>
constexpr vector<T, 3> rotate(quaternion<T> const &q, vector<T, 3> const &v) {
    vector<T, 3> const u{q.x(), q.y(), q.z()};
    vector<T, 3> const t = cross(u, v) * T{2};
    return v + t * q.w() + cross(u, t);
}

}  // namespace math
