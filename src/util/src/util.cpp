#include <dman/display.hpp>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <dman/config.hpp>
#include <dman/help.hpp>
#include <set>

void print_usage(const char *name)
{
    std::cout << "Usage: " << name << " [options]\n";

    std::string help_str(reinterpret_cast<const char *>(help_txt),
                         help_txt_len);

    std::cout << help_str;
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

void write_file(const std::string &file_path, const std::string &content)
{
    if (file_path == "-")
    {
        std::cout << content;
        return;
    }
    std::ofstream file_stream(file_path, std::ios::out | std::ios::binary);
    if (!file_stream)
        throw std::runtime_error("Failed to open file: " + file_path);
    file_stream << content;
}

std::string read_stdin_line()
{
    std::string line;
    std::getline(std::cin, line);
    return line;
}

std::string get_argument_name(std::string arg)
{
    if (arg == "-")
        return read_stdin_line();
    return arg;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_usage(argv[0]);
        return 0;
    }

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"toggle", required_argument, 0, 't'},
        {"enable", required_argument, 0, 'e'},
        {"disable", required_argument, 0, 'd'},
        {"list-config-outputs", required_argument, 0, 'c'},
        {"list-active-outputs", no_argument, 0, 'a'},
        {0, 0, 0, 0},
    };

    std::string input_file;
    std::string output_file;
    std::vector<std::string> toggle_outputs;
    std::vector<std::string> enable_outputs;
    std::vector<std::string> disable_outputs;
    std::vector<std::string> list_config_outputs;
    bool list_active_outputs = false;
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
        case 't':
            toggle_outputs.emplace_back(get_argument_name(optarg));
            break;
        case 'e':
            enable_outputs.emplace_back(get_argument_name(optarg));
            break;
        case 'd':
            disable_outputs.emplace_back(get_argument_name(optarg));
            break;
        case 'a':
            list_active_outputs = true;
            break;
        case 'c':
            list_config_outputs.emplace_back(get_argument_name(optarg));
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!toggle_outputs.empty() || !enable_outputs.empty() ||
        !disable_outputs.empty())
    {
        if (input_file.empty())
        {
            throw std::runtime_error(
                "Input file must be specified when toggling/enabling/disabling "
                "outputs.");
        }

        util::display::config cfg_input(read_file(input_file));
        util::display::config cfg_current(display::get_outputs());
        cfg_current.set_reference(cfg_input);

        for (const auto &name : toggle_outputs)
        {
            cfg_current.toggle_output(name);
        }
        for (const auto &name : enable_outputs)
        {
            cfg_current.enable_output(name);
        }
        for (const auto &name : disable_outputs)
        {
            cfg_current.disable_output(name);
        }

        std::cerr << (std::string)cfg_current;

        display::set_outputs(cfg_current);

        return 0;
    }

    if (list_active_outputs || list_config_outputs.size() > 0)
    {
        std::set<std::string> output_names;
        std::set<std::string> output_edids;

        for (const std::string &file : list_config_outputs)
        {
            util::display::config cfg_input(read_file(file));
            for (const auto &[edid, state] : cfg_input.outputs)
            {
                if (output_edids.find(edid) != output_edids.end())
                    continue;

                output_names.insert(cfg_input.get_name(edid));
                output_edids.insert(edid);
            }
        }

        if (list_active_outputs)
        {
            std::vector<display::output> outputs = display::get_outputs();
            for (const display::output &output : outputs)
            {
                std::string edid = output.edid.digest.hex();

                if (!output.is_active)
                    continue;

                if (output_edids.find(edid) != output_edids.end())
                    continue;

                output_names.insert(output.edid.name);
                output_edids.insert(edid);
            }
        }

        for (const std::string &name : output_names)
        {
            std::cout << name << std::endl;
        }

        return 0;
    }

    if (!input_file.empty())
    {
        util::display::config cfg(read_file(input_file));
        display::set_outputs(cfg);
    }

    if (!output_file.empty())
    {
        std::vector<display::output> outputs = display::get_outputs();
        util::display::config cfg(outputs);
        write_file(output_file, (std::string)cfg);
    }
    return 0;
}
