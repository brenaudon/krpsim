/*!
 *  @file krpsim_verif.cpp
 *  @brief Main entry point for the krpsim_verif executable.
 *
 *  This file serves as the main entry point for the krpsim_verif executable, which
 *  parses a configuration file and the result file and runs the verification (check if process can be run
 *  when it is launch in the trace).
 */

#include "krpsim.hpp"
#include "parsing.hpp"

int main(int argc, char **argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <config_file> <result_to_test>\n";
            return EXIT_FAILURE;
        }

        std::ifstream in(argv[1]);
        if (!in) {
            std::cerr << "Cannot open " << argv[1] << "\n";
            return EXIT_FAILURE;
        }
        Config cfg = parse_config(in);


        std::ifstream trace_in(argv[2]);
        if (!trace_in) {
            std::cerr << "Cannot open trace " << argv[2] << ".\n";
            return EXIT_FAILURE;
        }

        std::cout << "Verifying trace " << argv[2] << "...\n";


    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}