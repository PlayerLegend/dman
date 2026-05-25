#include <dman/display.hpp>
#include <cmath>
#include <dman/exception.hpp>
#include <filesystem>
#include <fstream>

namespace display
{

bool display::mode::operator==(const display::mode &other) const
{
    return (width == other.width) && (height == other.height) &&
           std::fabs(rate - other.rate) < 1.5;
}

static std::vector<uint8_t> s_read_binary(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);

    if (!stream)
        throw common::not_found(path);

    std::streamsize size = stream.tellg();

    if (size == -1)
        throw common::not_found(path);

    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> result;

    result.resize(size);

    stream.read((char *)result.data(), size);

    if (!stream.good())
        throw common::not_found(path);

    return result;
}

static std::unordered_map<std::string, std::vector<uint8_t>> s_list_edids()
{
    std::unordered_map<std::string, std::vector<uint8_t>> result;

    for (const auto &entry :
         std::filesystem::directory_iterator("/sys/class/drm"))
    {
        std::filesystem::path path = entry.path();
        std::filesystem::path edid_path = path / "edid";

        if (!std::filesystem::exists(edid_path))
            continue;

        std::string basename = path.filename();

        const auto dash = basename.find('-');

        if (dash == std::string::npos)
            throw common::exception("Unexpected name in /sys/class/drm");

        std::string name = basename.substr(dash + 1);

        result[name] = s_read_binary(edid_path);
    }

    return result;
}

} // namespace display
