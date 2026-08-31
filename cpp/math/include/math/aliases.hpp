#pragma once

#include <math/box.hpp>
#include <math/matrix.hpp>
#include <math/quaternion.hpp>
#include <math/vector.hpp>

namespace math {

using vector2f = vector<float, 2>;
using vector3f = vector<float, 3>;
using vector4f = vector<float, 4>;

using matrix2f = matrix<float, 2, 2>;
using matrix3f = matrix<float, 3, 3>;
using matrix4f = matrix<float, 4, 4>;

using box2f = box<float, 2>;
using box3f = box<float, 3>;
using box4f = box<float, 4>;

using quaternionf = quaternion<float>;

}  // namespace math
