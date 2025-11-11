#include <dman/config.hpp>
#include <dman/display.hpp>
#include <sstream>

std::string strip_whitespace(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos)
        return "";
    return str.substr(start, end - start + 1);
}

std::string get_next_nonempty_line(std::stringstream &ss)
{
    std::string line;
    while (std::getline(ss, line))
    {
        line = strip_whitespace(line);
        if (!line.empty())
        {
            return line;
        }
    }
    return "";
}

std::vector<std::string> split_words(const std::string &line)
{
    std::vector<std::string> words;
    std::istringstream iss(line);
    std::string word;
    while (iss >> word)
    {
        words.push_back(word);
    }
    return words;
}

util::display::config::config(const std::string &config_text)
{
    std::stringstream ss(config_text);
    while (ss)
    {
        std::vector<std::string> args = split_words(get_next_nonempty_line(ss));

        if (args.empty())
            continue;

        const std::string &edid = args[0];

        ::display::state &state = outputs[edid];

        state.is_active = true;

        for (int i = 1; i < args.size(); ++i)
        {
            const std::string &arg = args[i];
            size_t equal_pos = arg.find('=');

            if (equal_pos == std::string::npos)
            {
                if (arg == "primary")
                    state.is_primary = true;
                continue;
            }
            std::string key = arg.substr(0, equal_pos);
            std::string value = arg.substr(equal_pos + 1);
            if (key == "x")
            {
                state.position.x = std::stoi(value);
            }
            else if (key == "y")
            {
                state.position.y = std::stoi(value);
            }
            else if (key == "width")
            {
                state.mode.width = std::stoi(value);
            }
            else if (key == "height")
            {
                state.mode.height = std::stoi(value);
            }
            else if (key == "rate")
            {
                state.mode.rate = std::stod(value);
            }
            else if (key == "name")
            {
                associate_name_edid(value, edid);
            }
            else if (key == "rotation")
            {
                if (value == "normal")
                    state.rotation = ::display::rotation::NORMAL;
                else if (value == "left")
                    state.rotation = ::display::rotation::LEFT;
                else if (value == "right")
                    state.rotation = ::display::rotation::RIGHT;
                else if (value == "inverted")
                    state.rotation = ::display::rotation::INVERTED;
            }
        }
    }
}

void util::display::config::associate_name_edid(const std::string &name,
                                                const std::string &edid)
{
    name_to_edid[name] = edid;
    edid_to_name[edid] = name;
}

util::display::config::config(const std::vector<::display::output> &outputs)
{
    for (const ::display::output &output : outputs)
    {
        std::string hex = output.edid.digest.hex();
        if (output.is_active)
        {
            associate_name_edid(output.edid.name, hex);
            this->outputs[hex] = output;
        }
    }
}

std::string util::display::config::get_edid(const std::string &id) const
{
    auto it = name_to_edid.find(id);
    if (it != name_to_edid.end())
    {
        return it->second;
    }
    return id;
}

std::string util::display::config::get_name(const std::string &id) const
{
    auto it = edid_to_name.find(id);
    if (it != edid_to_name.end())
    {
        return it->second;
    }
    return id;
}

const ::display::state &
util::display::config::operator[](const std::string &name) const
{
    std::string edid = get_edid(name);
    auto it = outputs.find(edid);
    if (it != outputs.end())
        return it->second;

    static ::display::state empty_state;
    return empty_state;
}
util::display::config::operator std::string() const
{
    std::ostringstream oss;
    for (const auto &[edid, state] : outputs)
    {
        if (!state.is_active)
            continue;

        oss << edid;
        oss << " x=" << state.position.x;
        oss << " y=" << state.position.y;

        oss << " width=" << state.mode.width;
        oss << " height=" << state.mode.height;
        oss << " rate=" << state.mode.rate;

        const auto it = edid_to_name.find(edid);

        if (it != edid_to_name.end())
        {
            const std::string &name = it->second;
            oss << " name=" << name;
        }
        oss << " rotation=";
        switch (state.rotation)
        {
        case ::display::rotation::NORMAL:
            oss << "normal";
            break;
        case ::display::rotation::LEFT:
            oss << "left";
            break;
        case ::display::rotation::RIGHT:
            oss << "right";
            break;
        case ::display::rotation::INVERTED:
            oss << "inverted";
            break;
        }
        if (state.is_primary)
        {
            oss << " primary";
        }
        oss << "\n";
    }
    return oss.str();
}

void util::display::config::toggle_output(const std::string &name)
{
    if (name.empty())
        return;
    std::string edid = get_edid(name);
    auto it = outputs.find(edid);
    if (it == outputs.end())
        return;
    ::display::state &state = it->second;
    state.is_active = !state.is_active;
}

void util::display::config::enable_output(const std::string &name)
{
    if (name.empty())
        return;
    std::string edid = get_edid(name);
    auto it = outputs.find(edid);
    if (it == outputs.end())
        return;
    ::display::state &state = it->second;
    state.is_active = true;
}

void util::display::config::disable_output(const std::string &name)
{
    if (name.empty())
        return;
    std::string edid = get_edid(name);
    auto it = outputs.find(edid);
    if (it == outputs.end())
        return;
    ::display::state &state = it->second;
    state.is_active = false;
}

void util::display::config::set_reference(const util::display::config &other)
{
    for (const auto &[edid, other_state] : other.outputs)
    {
        ::display::state &have_state = outputs[edid];
        if (have_state.is_active)
            continue;
        have_state = other_state;
        have_state.is_active = false;
    }

    for (const auto &[name, edid] : other.name_to_edid)
    {
        associate_name_edid(name, edid);
    }
}