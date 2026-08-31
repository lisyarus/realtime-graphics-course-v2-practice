#pragma once

#include <math/aliases.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>

struct ObjVertex {
    math::vector3f position;
    math::vector3f normal;
    math::vector2f texcoord;
};

struct ObjMesh {
    std::vector<ObjVertex> vertices;
    std::vector<std::uint32_t> indices;
};

ObjMesh loadObj(std::filesystem::path const &path);
