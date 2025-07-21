/*!
 *  @file main.cpp
 *  @brief Main entry point for the krpsim executable.
 *
 *  This file serves as the main entry point for the krpsim executable, which
 *  parses a configuration file and runs the simulation based on the parsed
 *  configuration.
 */

#include "parsing.hpp"
#include "helper.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config-file>\n";
        return EXIT_FAILURE;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return EXIT_FAILURE;
    }

    try {
        Config cfg = parse_config(in);
        print_config(cfg);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}