#include "../../src/evdev.hpp"
#include <dman/digest.hpp>
#include <iostream>

int main()
{
    std::vector<std::string> devices = evdev::list_devices();

    for (const std::string &device_path : devices)
    {
        std::cout << "Device: " << device_path << std::endl;
        evdev::device dev(device_path);
        digest::sha256 hash = dev;
        std::string info = dev;
        std::cout << "Hash: " << hash.hex() << "\n" << info << std::endl;
    }

    return 0;
}