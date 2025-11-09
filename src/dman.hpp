#include "digest.hpp"
#include <vector>

namespace display
{
struct mode
{
    std::string name;
    unsigned int width;
    unsigned int height;
    double rate;
    bool operator==(const mode &other) const;
};
struct edid
{
    digest::sha256 digest;
    std::vector<uint8_t> raw;
    edid() {};
    edid(const void *data, size_t size);
};
enum class rotation : uint8_t
{
    NORMAL,
    LEFT,
    RIGHT,
    INVERTED,
};

template <typename T> class vec2
{
  public:
    T x;
    T y;
};

class output
{
  public:
    std::string name;
    std::vector<mode> modes;
    vec2<unsigned int> position;
    uint32_t mode_index = 0;
    bool is_primary = false;
    bool is_active = false;
    enum rotation rotation;
    class edid edid;
};
class session
{

  public:
    session() {};
    operator std::vector<output>();
    void operator=(const std::vector<output> &outputs);
};

} // namespace display