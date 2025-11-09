#include "dman.hpp"
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

void print_usage(const char *prog_name)
{
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -i, --input FILE      Input configuration file\n"
              << "  -o, --output FILE     Output configuration file\n";
}

std::string read_stdin()
{
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

std::string read_file(const std::string &file_path)
{
    if (file_path == "-")
    {
        return read_stdin();
    }
    std::ifstream file_stream(file_path, std::ios::in | std::ios::binary);
    if (!file_stream)
        throw std::runtime_error("Failed to open file: " + file_path);
    std::ostringstream ss;
    ss << file_stream.rdbuf();
    return ss.str();
}

std::string strip_whitespace(const std::string &str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos)
        return "";
    return str.substr(start, end - start + 1);
}

bool starts_with_space(const std::string &str)
{
    return !str.empty() && std::isspace(str[0]);
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

// class config_block
// {
//     std::string name;
//     std::vector<std::string> lines;

//   public:
//     config_block(std::stringstream &ss)
//     {
//         name = get_next_nonempty_line(ss);
//         std::string line;
//         while (std::getline(ss, line))
//         {
//             if (!starts_with_space(line))
//             {
//                 break;
//             }
//             lines.push_back(strip_whitespace(line));
//         }
//     }
// };

class config
{
  public:
    struct output
    {
        std::string edid_hex;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        double rate = 0;
        enum ::display::rotation rotation;
        bool primary = false;
    };

    std::unordered_map<std::string, config::output> outputs;
    config() {};
    config(const std::string &config_text)
    {
        std::stringstream ss(config_text);
        while (ss)
        {
            std::vector<std::string> args =
                split_words(get_next_nonempty_line(ss));

            if (args.empty())
                continue;

            config::output output;

            output.edid_hex = args[0];

            for (int i = 1; i < args.size(); ++i)
            {
                const std::string &arg = args[i];
                size_t equal_pos = arg.find('=');

                if (equal_pos == std::string::npos)
                {
                    if (arg == "primary")
                        output.primary = true;
                    continue;
                }
                std::string key = arg.substr(0, equal_pos);
                std::string value = arg.substr(equal_pos + 1);
                if (key == "x")
                {
                    output.x = std::stoi(value);
                }
                else if (key == "y")
                {
                    output.y = std::stoi(value);
                }
                else if (key == "width")
                {
                    output.width = std::stoi(value);
                }
                else if (key == "height")
                {
                    output.height = std::stoi(value);
                }
                else if (key == "rate")
                {
                    output.rate = std::stod(value);
                }
                else if (key == "rotation")
                {
                    if (value == "normal")
                        output.rotation = ::display::rotation::NORMAL;
                    else if (value == "left")
                        output.rotation = ::display::rotation::LEFT;
                    else if (value == "right")
                        output.rotation = ::display::rotation::RIGHT;
                    else if (value == "inverted")
                        output.rotation = ::display::rotation::INVERTED;
                }
            }

            outputs[output.edid_hex] = output;
        }
    }

    config(std::vector<display::output> &outputs)
    {
        for (const display::output &output : outputs)
        {
            config::output cfg_output;
            cfg_output.edid_hex = output.edid.digest.hex();
            cfg_output.x = output.position.x;
            cfg_output.y = output.position.y;
            cfg_output.width = output.modes[output.mode_index].width;
            cfg_output.height = output.modes[output.mode_index].height;
            cfg_output.rate = output.modes[output.mode_index].rate;
            cfg_output.rotation = output.rotation;
            cfg_output.primary = output.is_primary;
            this->outputs[cfg_output.edid_hex] = cfg_output;
        }
    }

    std::string serialize() const
    {
        std::ostringstream oss;
        for (const auto &pair : outputs)
        {
            const config::output &output = pair.second;
            oss << output.edid_hex;
            oss << " x=" << output.x;
            oss << " y=" << output.y;
            oss << " width=" << output.width;
            oss << " height=" << output.height;
            oss << " rate=" << output.rate;
            oss << " rotation=";
            switch (output.rotation)
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
            if (output.primary)
            {
                oss << " primary";
            }
            oss << "\n";
        }
        return oss.str();
    }
};

uint32_t find_mode_index(const std::vector<display::mode> &modes,
                         const config::output &cfg_output)
{
    for (size_t i = 0, size = modes.size(); i < size; ++i)
    {
        const display::mode &mode = modes[i];
        if (mode.width == cfg_output.width &&
            mode.height == cfg_output.height &&
            (cfg_output.rate == 0 || mode.rate == cfg_output.rate))
        {
            return static_cast<uint32_t>(i);
        }
    }
    std::cerr << "Warning: Target mode not found in mode list." << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0},
    };

    std::string input_file;
    std::string output_file;
    int option_index = 0;
    int c;
    while (
        (c = getopt_long(argc, argv, "hi:o:", long_options, &option_index)) !=
        -1)
    {
        switch (c)
        {
        case 'h':
            print_usage(argv[0]);
            return 0;
        case 'i':
            input_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input_file.empty())
    {
        config cfg(read_file(input_file));
        display::session sess;
        std::vector<display::output> outputs = sess;
        for (display::output &output : outputs)
        {
            std::string edid_hex = output.edid.digest.hex();
            auto it = cfg.outputs.find(edid_hex);
            if (it == cfg.outputs.end())
            {
                output.is_active = false;
                continue;
            }
            const config::output &cfg_output = it->second;
            output.position.x = cfg_output.x;
            output.position.y = cfg_output.y;
            output.rotation = cfg_output.rotation;
            output.is_primary = cfg_output.primary;
            output.is_active = true;
            output.mode_index = find_mode_index(output.modes, cfg_output);
        }
        sess = outputs;
    }

    if (!output_file.empty())
    {
        display::session sess;
        std::vector<display::output> outputs = sess;
        config cfg(outputs);
        std::string serialized_cfg = cfg.serialize();
        if (output_file == "-")
        {
            std::cout << serialized_cfg;
        }
        else
        {
            std::ofstream ofs(output_file, std::ios::out | std::ios::binary);
            if (!ofs)
            {
                std::cerr << "Error: Failed to open output file: "
                          << output_file << std::endl;
                return 1;
            }
            ofs << serialized_cfg;
        }
    }
    return 0;
}
