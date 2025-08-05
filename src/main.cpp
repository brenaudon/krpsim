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

        std::vector<Process> proc_to_remove;
        for (const auto &proc : cfg.processes) {
            if (proc.results.empty()) {
                proc_to_remove.push_back(proc);
            }
        }
        // Remove processes with no results
        for (Process proc : proc_to_remove) {
            auto it = std::remove_if(cfg.processes.begin(), cfg.processes.end(),
                                     [&proc](const Process &p) { return p.name == proc.name; });
            cfg.processes.erase(it, cfg.processes.end());
        }

        Candidate best_candidate = solve_with_ga(cfg, 50000);
        std::cout << "Simulation trace:\n";
        for (const auto &entry : best_candidate.trace) {
            std::cout << "Cycle " << entry.cycle << ": Start process " << cfg.processes[entry.procId].name << '\n';
        }

        std::cout << '\n';
        std::cout << "Total cycles:" << best_candidate.cycle << "\n";

        std::cout << '\n';
        std::cout << "Final stocks:\n";
        for (const auto &stock : best_candidate.stocks) {
            std::cout << stock.first << ": " << stock.second << '\n';
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}