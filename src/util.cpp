#include "dman.hpp"
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include "config.hpp"

void print_usage(const char *prog_name)
{
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -i, --input FILE      Input configuration file\n"
              << "  -o, --output FILE     Output configuration file\n"
              << "  -t, --toggle NAME    Toggle output by name\n"
              << "  -e, --enable NAME    Enable output by name\n"
              << "  -d, --disable NAME   Disable output by name\n";
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

void write_file(const std::string &file_path,
                    const std::string &content)
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

int main(int argc, char *argv[])
{
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"input", required_argument, 0, 'i'},
        {"output", required_argument, 0, 'o'},
        {"toggle", required_argument, 0, 0},
        {"enable", required_argument, 0, 0},
        {"disable", required_argument, 0, 0},
        {0, 0, 0, 0},
    };

    std::string input_file;
    std::string output_file;
    std::vector<std::string> toggle_outputs;
    std::vector<std::string> enable_outputs;
    std::vector<std::string> disable_outputs;
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
