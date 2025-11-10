#include "config.hpp"
#include "dman.hpp"
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

        ::display::state &state = outputs[args[0]];

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
                state.mode.name = value;
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

util::display::config::config(const std::vector<::display::output> &outputs)
{
    for (const ::display::output &output : outputs)
    {
        std::string hex = output.edid.digest.hex();
        this->name_to_edid[output.edid.name] = hex;
        this->edid_to_name[hex] = output.edid.name;
        if (output.is_active)
            this->outputs[hex] = output;
    }
}

const ::display::state &
util::display::config::operator[](const std::string &name) const
{
    auto it_result = outputs.find(name);
    if (it_result != outputs.end())
        return it_result->second;

    auto it_name = name_to_edid.find(name);
    if (it_name != name_to_edid.end())
    {
        auto it_result2 = outputs.find(it_name->second);
        if (it_result2 != outputs.end())
            return it_result2->second;
    }

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
            oss << " name=" << it->second;
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