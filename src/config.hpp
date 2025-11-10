#include <unordered_map>
#include <string>
#include <vector>

namespace display
{
struct mode;
struct state;
struct output;
} // namespace display

namespace util::display
{

struct config
{
    std::unordered_map<std::string, ::display::state> outputs;
    std::unordered_map<std::string, std::string> name_to_edid;
    std::unordered_map<std::string, std::string> edid_to_name;

  public:
    config(const std::vector<::display::output> &outputs);
    config(const std::string &config_text);
    operator std::string() const;
    operator const std::unordered_map<std::string, ::display::state> &() const
    {
        return outputs;
    }
    const ::display::state & operator[](const std::string &name) const;
};
} // namespace util::display