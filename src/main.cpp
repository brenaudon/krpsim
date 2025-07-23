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
#include "krpsim.hpp"
#include "compute.hpp"

/**
 * @brief Main function for the krpsim executable.
 *
 * This function reads a configuration file specified as a command line argument,
 * parses it, and runs the simulation. If an error occurs during parsing,
 * it catches the exception and prints an error message.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
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
        std::cout << "Running beam search with width 100 and max iterations 10000..." << std::endl;
        std::cout << beam_search(cfg, 100, 10000) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}