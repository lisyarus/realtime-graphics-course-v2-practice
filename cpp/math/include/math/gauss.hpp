#pragma once

#include <math/matrix.hpp>
#include <math/vector.hpp>

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace math {

namespace detail {

template <typename T, std::size_t N>
constexpr void row_swap(vector<T, N> &v, std::size_t i, std::size_t j) {
    std::swap(v[i], v[j]);
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr void row_swap(matrix<T, Rows, Cols> &m, std::size_t i, std::size_t j) {
    for (std::size_t c = 0; c < Cols; ++c) {
        std::swap(m[i][c], m[j][c]);
    }
}

template <typename T, std::size_t N>
constexpr void row_divide(vector<T, N> &v, std::size_t i, T s) {
    v[i] /= s;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr void row_divide(matrix<T, Rows, Cols> &m, std::size_t i, T s) {
    for (std::size_t c = 0; c < Cols; ++c) {
        m[i][c] /= s;
    }
}

template <typename T, std::size_t N>
constexpr void row_subtract_scaled(vector<T, N> &v, std::size_t dst, std::size_t src, T factor) {
    v[dst] -= factor * v[src];
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr void row_subtract_scaled(matrix<T, Rows, Cols> &m, std::size_t dst, std::size_t src,
                                   T factor) {
    for (std::size_t c = 0; c < Cols; ++c) {
        m[dst][c] -= factor * m[src][c];
    }
}

// Reduces a to identity, applying the same row operations to rhs,
// which is either a vector (single right-hand side) or a matrix (several of them).
template <typename T, std::size_t N, typename Rhs>
bool gauss_jordan(matrix<T, N, N> &a, Rhs &rhs) {
    using std::abs;
    for (std::size_t k = 0; k < N; ++k) {
        std::size_t pivot_row = k;
        for (std::size_t i = k + 1; i < N; ++i) {
            if (abs(a[i][k]) > abs(a[pivot_row][k])) {
                pivot_row = i;
            }
        }
        if (a[pivot_row][k] == T{}) {
            return false;
        }
        if (pivot_row != k) {
            row_swap(a, pivot_row, k);
            row_swap(rhs, pivot_row, k);
        }
        T const pivot = a[k][k];
        row_divide(a, k, pivot);
        row_divide(rhs, k, pivot);
        for (std::size_t i = 0; i < N; ++i) {
            if (i == k) {
                continue;
            }
            T const factor = a[i][k];
            row_subtract_scaled(a, i, k, factor);
            row_subtract_scaled(rhs, i, k, factor);
        }
    }
    return true;
}

}  // namespace detail

template <typename T, std::size_t N>
std::optional<vector<T, N>> solve(matrix<T, N, N> a, vector<T, N> b) {
    if (!detail::gauss_jordan(a, b)) {
        return std::nullopt;
    }
    return b;
}

template <typename T, std::size_t N>
std::optional<matrix<T, N, N>> inverse(matrix<T, N, N> a) {
    matrix<T, N, N> result = matrix<T, N, N>::identity();
    if (!detail::gauss_jordan(a, result)) {
        return std::nullopt;
    }
    return result;
}

}  // namespace math
