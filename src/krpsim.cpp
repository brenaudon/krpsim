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
#include "genetic_algo.hpp"


/**
 * @brief Convert a delay string in seconds to an int of milliseconds.
 *
 * This function takes a string representing a delay in seconds, converts it to an integer,
 * and then multiplies it by 1000 to convert it to milliseconds.
 *
 * @param delay_str The string representing the delay in seconds.
 * @return The delay in milliseconds.
 * @throws std::runtime_error if the string cannot be converted to an integer.
 */
int delay_to_ms(const char *delay_str) {
    int delay = 0;
    auto [ptr, ec] = std::from_chars(delay_str, delay_str + std::strlen(delay_str), delay);
    if (ec != std::errc{}) {
        throw std::runtime_error("Invalid delay value: " + std::string(delay_str));
    }
    return delay * 1000; // Convert seconds to milliseconds
}

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
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <config-file> <delay_in_sec>\n";
        return EXIT_FAILURE;
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "Cannot open " << argv[1] << "\n";
        return EXIT_FAILURE;
    }


    try {
        int delay = delay_to_ms(argv[2]);
        Config cfg = parse_config_for_simulation(in);
        //print_config(cfg);

        std::cout << "\nInitial stocks:\n";
        for (const auto &pair : cfg.initialStocks) {
            std::cout << pair.first << ": " << pair.second << '\n';
        }

        Candidate best_candidate = solve_with_ga(cfg, delay);

        std::cout << "\nSimulation trace:\n";
        for (const auto &entry : best_candidate.trace) {
            std::cout << entry.cycle << ":" << cfg.processes[entry.procId].name << '\n';
        }
        std::cout << "\nTotal cycles:" << best_candidate.cycle << "\n";

        std::cout << "\nFinal stock:\n";
        for (size_t i = 0; i < best_candidate.stocks_by_id.size(); ++i) {
            std::string item_name = cfg.id_to_item[i];
            int qty = best_candidate.stocks_by_id[i];
            std::cout << item_name << ": " << qty << '\n';
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}