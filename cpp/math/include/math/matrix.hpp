#pragma once

#include <math/vector.hpp>

#include <cstddef>

namespace math {

template <typename T, std::size_t Rows, std::size_t Cols>
struct matrix {
    T data[Rows][Cols];

    constexpr matrix() = default;

    constexpr T (&operator[](std::size_t row))[Cols] { return data[row]; }
    constexpr T const (&operator[](std::size_t row) const)[Cols] { return data[row]; }

    constexpr T &operator()(std::size_t row, std::size_t col) { return data[row][col]; }
    constexpr T const &operator()(std::size_t row, std::size_t col) const {
        return data[row][col];
    }

    static constexpr matrix identity() requires (Rows == Cols) {
        matrix m{};
        for (std::size_t i = 0; i < Rows; ++i) {
            m[i][i] = T{1};
        }
        return m;
    }

    friend constexpr bool operator==(matrix const &, matrix const &) = default;
};

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> &operator+=(matrix<T, Rows, Cols> &a,
                                            matrix<T, Rows, Cols> const &b) {
    for (std::size_t r = 0; r < Rows; ++r) {
        for (std::size_t c = 0; c < Cols; ++c) {
            a[r][c] += b[r][c];
        }
    }
    return a;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> &operator-=(matrix<T, Rows, Cols> &a,
                                            matrix<T, Rows, Cols> const &b) {
    for (std::size_t r = 0; r < Rows; ++r) {
        for (std::size_t c = 0; c < Cols; ++c) {
            a[r][c] -= b[r][c];
        }
    }
    return a;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> &operator*=(matrix<T, Rows, Cols> &a, T s) {
    for (std::size_t r = 0; r < Rows; ++r) {
        for (std::size_t c = 0; c < Cols; ++c) {
            a[r][c] *= s;
        }
    }
    return a;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> operator+(matrix<T, Rows, Cols> a,
                                          matrix<T, Rows, Cols> const &b) {
    return a += b;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> operator-(matrix<T, Rows, Cols> a,
                                          matrix<T, Rows, Cols> const &b) {
    return a -= b;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> operator*(matrix<T, Rows, Cols> m, T s) {
    return m *= s;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Rows, Cols> operator*(T s, matrix<T, Rows, Cols> m) {
    return m *= s;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr vector<T, Rows> operator*(matrix<T, Rows, Cols> const &m, vector<T, Cols> const &v) {
    vector<T, Rows> result;
    for (std::size_t r = 0; r < Rows; ++r) {
        T sum{};
        for (std::size_t c = 0; c < Cols; ++c) {
            sum += m[r][c] * v[c];
        }
        result[r] = sum;
    }
    return result;
}

template <typename T, std::size_t Rows, std::size_t Mid, std::size_t Cols>
constexpr matrix<T, Rows, Cols> operator*(matrix<T, Rows, Mid> const &a,
                                          matrix<T, Mid, Cols> const &b) {
    matrix<T, Rows, Cols> result;
    for (std::size_t r = 0; r < Rows; ++r) {
        for (std::size_t c = 0; c < Cols; ++c) {
            T sum{};
            for (std::size_t k = 0; k < Mid; ++k) {
                sum += a[r][k] * b[k][c];
            }
            result[r][c] = sum;
        }
    }
    return result;
}

template <typename T, std::size_t Rows, std::size_t Cols>
constexpr matrix<T, Cols, Rows> transpose(matrix<T, Rows, Cols> const &m) {
    matrix<T, Cols, Rows> result;
    for (std::size_t r = 0; r < Rows; ++r) {
        for (std::size_t c = 0; c < Cols; ++c) {
            result[c][r] = m[r][c];
        }
    }
    return result;
}

}  // namespace math
