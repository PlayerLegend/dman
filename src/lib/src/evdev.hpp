#include <libevdev/libevdev.h>
#include <string>
#include <vector>

namespace digest { class sha256; }

namespace evdev
{
class device
{
    struct libevdev *dev;

  public:
    device(int fd);
    device(const std::string &path);
    ~device();

    std::string get_name() const;
    std::string get_phys() const;
    std::string get_uniq() const;
    int get_id_bustype() const;
    int get_id_vendor() const;
    int get_id_product() const;
    int get_id_version() const;

    operator std::string () const;
    operator digest::sha256() const;
};

std::vector<std::string> list_devices();

} // namespace evdev