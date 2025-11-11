#include <dman/digest.hpp>
#include <vector>
#include <unordered_map>

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

namespace util::display
{
struct config;
}

struct edid
{
    digest::sha256 digest;
    std::vector<uint8_t> raw;
    std::string manufacturer_id;
    std::string manufacturer_product_code;
    std::string serial_number;
    std::string name;
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

struct state
{
    struct mode mode;
    vec2<unsigned int> position;
    enum rotation rotation;
    bool is_primary;
    bool is_active;
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
    void operator=(const mode &mode);
    void operator=(const state &state);
    operator state() const;
};

std::vector<output> get_outputs();
void set_outputs(const std::unordered_map<std::string, display::state> &);

// class session
// {
//   public:
//     session() {};
//     operator std::vector<output>();
//     void operator=(const std::unordered_map<std::string, state> &);
//     void operator=(const util::display::config &);
// };

} // namespace display