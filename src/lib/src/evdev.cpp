#include "evdev.hpp"
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <dman/digest.hpp>
#include <filesystem>
#include <errno.h>
#include <string.h>

namespace evdev
{
device::device(int fd)
{
    if (libevdev_new_from_fd(fd, &dev) < 0)
    {

        throw std::runtime_error("libevdev_new_from_fd: " +
                                 std::string(strerror(errno)));
    }
}

device::device(const std::string &path)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        throw std::runtime_error("Failed to open evdev fd: " +
                                 std::string(strerror(errno)));
    }
    if (libevdev_new_from_fd(fd, &dev) < 0)
    {
        close(fd);
        throw std::runtime_error("libevdev_new_from_fd: " +
                                 std::string(strerror(errno)));
    }
    close(fd);
}

device::~device()
{
    libevdev_free(dev);
}

std::string device::get_name() const
{
    const char *s = libevdev_get_name(dev);
    return s ? std::string(s) : std::string();
}

std::string device::get_phys() const
{
    const char *s = libevdev_get_phys(dev);
    return s ? std::string(s) : std::string();
}

std::string device::get_uniq() const
{
    const char *s = libevdev_get_uniq(dev);
    return s ? std::string(s) : std::string();
}

int device::get_id_bustype() const
{
    return libevdev_get_id_bustype(dev);
}

int device::get_id_vendor() const
{
    return libevdev_get_id_vendor(dev);
}

int device::get_id_product() const
{
    return libevdev_get_id_product(dev);
}

int device::get_id_version() const
{
    return libevdev_get_id_version(dev);
}

evdev::device::operator std::string() const
{
    return "Name: " + get_name() + "\n" +                             //
           "Uniq: " + get_uniq() + "\n" +                             //
           "Vendor ID: " + std::to_string(get_id_vendor()) + "\n" +   //
           "Product ID: " + std::to_string(get_id_product()) + "\n" + //
           "Version: " + std::to_string(get_id_version()) + "\n";
}

evdev::device::operator digest::sha256() const
{
    std::string info = *this;
    return digest::sha256(info);
}

std::vector<std::string> list_devices()
{
    std::vector<std::string> result;
    const std::string input_path = "/dev/input";

    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(input_path))
    {
        if (entry.is_character_file() &&
            entry.path().filename().string().find("event") == 0)
        {
            result.emplace_back(entry.path().string());
        }
    }

    return result;
}

} // namespace evdev