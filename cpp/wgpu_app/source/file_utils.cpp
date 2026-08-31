#include <file_utils.hpp>

#include <fstream>
#include <stdexcept>

std::string loadFile(std::filesystem::path const &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open " + path.string());
    }
    auto const fileSize = file.tellg();
    file.seekg(std::ios::beg);
    std::string result(fileSize, '\0');
    file.read(result.data(), result.size());
    return result;
}