#pragma once

#include <math/matrix.hpp>
#include <math/quaternion.hpp>
#include <math/vector.hpp>

#include <cstddef>

namespace math {

template <typename T, std::size_t N>
constexpr matrix<T, N + 1, N + 1> translation(vector<T, N> const &t) {
    auto m = matrix<T, N + 1, N + 1>::identity();
    for (std::size_t i = 0; i < N; ++i) {
        m[i][N] = t[i];
    }
    return m;
}

template <typename T, std::size_t N>
constexpr matrix<T, N + 1, N + 1> scale(vector<T, N> const &s) {
    auto m = matrix<T, N + 1, N + 1>::identity();
    for (std::size_t i = 0; i < N; ++i) {
        m[i][i] = s[i];
    }
    return m;
}

template <typename T>
constexpr matrix<T, 4, 4> rotation(quaternion<T> const &q) {
    T const xx = q.x() * q.x();
    T const yy = q.y() * q.y();
    T const zz = q.z() * q.z();
    T const xy = q.x() * q.y();
    T const xz = q.x() * q.z();
    T const yz = q.y() * q.z();
    T const wx = q.w() * q.x();
    T const wy = q.w() * q.y();
    T const wz = q.w() * q.z();

    auto m = matrix<T, 4, 4>::identity();
    m[0][0] = T{1} - T{2} * (yy + zz);
    m[0][1] = T{2} * (xy - wz);
    m[0][2] = T{2} * (xz + wy);
    m[1][0] = T{2} * (xy + wz);
    m[1][1] = T{1} - T{2} * (xx + zz);
    m[1][2] = T{2} * (yz - wx);
    m[2][0] = T{2} * (xz - wy);
    m[2][1] = T{2} * (yz + wx);
    m[2][2] = T{1} - T{2} * (xx + yy);
    return m;
}

}  // namespace math
