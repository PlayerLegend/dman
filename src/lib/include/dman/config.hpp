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
    std::string get_edid(const std::string &id) const;
    std::string get_name(const std::string &id) const;
    config(const std::vector<::display::output> &outputs);
    config(const std::string &config_text);
    void set_reference(const util::display::config &other);
    operator std::string() const;
    operator const std::unordered_map<std::string, ::display::state> &() const
    {
        return outputs;
    }
    const ::display::state &operator[](const std::string &name) const;
    void toggle_output(const std::string &name);
    void enable_output(const std::string &name);
    void disable_output(const std::string &name);
};
} // namespace util::display