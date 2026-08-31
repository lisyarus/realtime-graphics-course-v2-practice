#include <obj_loader.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr std::size_t maxFaceCorners = 16;

struct ObjIndex {
    int position = 0;
    int texcoord = 0;
    int normal = 0;
};

struct VertexKey {
    std::uint32_t position = 0;
    std::uint32_t texcoord = 0;
    std::uint32_t normal = 0;

    friend bool operator==(VertexKey const &, VertexKey const &) = default;
};

void hashCombine(std::size_t &seed, std::uint32_t value) {
    seed ^= static_cast<std::size_t>(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct VertexKeyHash {
    std::size_t operator()(VertexKey const &key) const {
        std::size_t seed = 0;
        hashCombine(seed, key.position);
        hashCombine(seed, key.texcoord);
        hashCombine(seed, key.normal);
        return seed;
    }
};

std::string readFile(std::filesystem::path const &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("failed to open " + path.string());
    }
    std::string contents(static_cast<std::size_t>(file.tellg()), '\0');
    file.seekg(0);
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    return contents;
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

void skipSpaces(char const *&p, char const *end) {
    while (p != end && isSpace(*p)) {
        ++p;
    }
}

void skipLine(char const *&p, char const *end) {
    while (p != end && *p != '\n') {
        ++p;
    }
    if (p != end) {
        ++p;
    }
}

[[noreturn]] void fail(char const *what, std::size_t lineNumber) {
    throw std::runtime_error(std::string(what) + " at line " + std::to_string(lineNumber));
}

template <typename T>
T parseNumber(char const *&p, char const *end, char const *what, std::size_t lineNumber) {
    skipSpaces(p, end);
    T value{};
    auto const [next, error] = std::from_chars(p, end, value);
    if (error != std::errc{}) {
        fail(what, lineNumber);
    }
    p = next;
    return value;
}

math::vector3f parseVector(char const *&p, char const *end, char const *what,
                           std::size_t lineNumber) {
    math::vector3f result;
    result.x() = parseNumber<float>(p, end, what, lineNumber);
    result.y() = parseNumber<float>(p, end, what, lineNumber);
    result.z() = parseNumber<float>(p, end, what, lineNumber);
    return result;
}

math::vector2f parseVector2(char const *&p, char const *end, char const *what,
                            std::size_t lineNumber) {
    math::vector2f result;
    result.x() = parseNumber<float>(p, end, what, lineNumber);
    result.y() = parseNumber<float>(p, end, what, lineNumber);
    return result;
}

// Parses "1", "1/2", "1//3" or "1/2/3".
ObjIndex parseFaceCorner(char const *&p, char const *end, std::size_t lineNumber) {
    ObjIndex index;
    index.position = parseNumber<int>(p, end, "invalid OBJ face", lineNumber);
    if (p == end || *p != '/') {
        return index;
    }

    ++p;
    if (p != end && *p != '/') {
        index.texcoord = parseNumber<int>(p, end, "invalid OBJ face", lineNumber);
    }
    if (p != end && *p == '/') {
        ++p;
        index.normal = parseNumber<int>(p, end, "invalid OBJ face", lineNumber);
    }
    return index;
}

std::uint32_t resolveIndex(int index, std::size_t size, std::size_t lineNumber) {
    long long const resolved = index > 0 ? static_cast<long long>(index - 1)
                                         : static_cast<long long>(size) + index;
    if (index == 0 || resolved < 0 || resolved >= static_cast<long long>(size)) {
        fail("invalid OBJ index", lineNumber);
    }
    return static_cast<std::uint32_t>(resolved);
}

}  // namespace

ObjMesh loadObj(std::filesystem::path const &path) {
    std::string const contents = readFile(path);

    std::vector<math::vector3f> positions;
    std::vector<math::vector2f> texcoords;
    std::vector<math::vector3f> normals;
    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> unique;
    ObjMesh mesh;

    char const *p = contents.data();
    char const *const end = p + contents.size();
    std::size_t lineNumber = 0;

    while (p != end) {
        ++lineNumber;
        skipSpaces(p, end);

        if (end - p >= 2 && p[0] == 'v' && isSpace(p[1])) {
            p += 2;
            positions.push_back(parseVector(p, end, "invalid OBJ vertex", lineNumber));
        } else if (end - p >= 3 && p[0] == 'v' && p[1] == 't' && isSpace(p[2])) {
            p += 3;
            texcoords.push_back(parseVector2(p, end, "invalid OBJ texcoord", lineNumber));
        } else if (end - p >= 3 && p[0] == 'v' && p[1] == 'n' && isSpace(p[2])) {
            p += 3;
            normals.push_back(parseVector(p, end, "invalid OBJ normal", lineNumber));
        } else if (end - p >= 2 && p[0] == 'f' && isSpace(p[1])) {
            p += 2;

            std::array<ObjIndex, maxFaceCorners> face;
            std::size_t cornerCount = 0;
            while (true) {
                skipSpaces(p, end);
                if (p == end || *p == '\n') {
                    break;
                }
                if (cornerCount == face.size()) {
                    fail("OBJ face has too many vertices", lineNumber);
                }
                face[cornerCount++] = parseFaceCorner(p, end, lineNumber);
            }
            if (cornerCount < 3) {
                fail("OBJ face has fewer than three vertices", lineNumber);
            }

            for (std::size_t i = 1; i + 1 < cornerCount; ++i) {
                for (ObjIndex const index : {face[0], face[i], face[i + 1]}) {
                    VertexKey const key{
                        resolveIndex(index.position, positions.size(), lineNumber),
                        index.texcoord == 0
                            ? 0
                            : resolveIndex(index.texcoord, texcoords.size(), lineNumber) + 1,
                        index.normal == 0
                            ? 0
                            : resolveIndex(index.normal, normals.size(), lineNumber) + 1,
                    };

                    auto const [it, inserted] = unique.emplace(
                        key, static_cast<std::uint32_t>(mesh.vertices.size()));
                    if (inserted) {
                        mesh.vertices.push_back(
                            {positions[key.position],
                             key.normal == 0 ? math::vector3f{} : normals[key.normal - 1],
                             key.texcoord == 0 ? math::vector2f{} : texcoords[key.texcoord - 1]});
                    }
                    mesh.indices.push_back(it->second);
                }
            }
        }

        skipLine(p, end);
    }

    return mesh;
}
