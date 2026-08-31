#pragma once

#include <math/detail/alloca.hpp>
#include <math/vector.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <span>
#include <type_traits>

namespace math {

template <typename Point, typename T>
Point in_place_bezier(std::span<Point> points, T t) {
    std::size_t const n = points.size();
    assert(n > 0);

    for (std::size_t k = n - 1; k > 0; --k) {
        for (std::size_t i = 0; i < k; ++i) {
            points[i] = lerp(points[i], points[i + 1], t);
        }
    }

    return points[0];
}

template <typename Points, typename T>
auto bezier(Points const &points, T t) {
    using std::begin;
    using std::end;
    using point = std::remove_cv_t<std::remove_reference_t<decltype(*begin(points))>>;
    static_assert(std::is_trivially_copyable_v<point>);

    std::size_t const n = static_cast<std::size_t>(points.size());
    assert(n > 0);

    point *scratch = math_alloca(point, n);
    std::copy(begin(points), end(points), scratch);
    return in_place_bezier(std::span<point>{scratch, n}, t);
}

}  // namespace math
