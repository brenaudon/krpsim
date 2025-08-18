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

void print_max_stocks(const Config &cfg) {
    std::cout << "Max stocks:\n";
    if (cfg.maxStocks.limiting_item.empty()) {
        std::cout << "  No limiting item\n";
    } else {
        std::cout << "  Limiting item: " << cfg.maxStocks.limiting_item
                  << " (initial stock: " << cfg.maxStocks.limiting_initial_stock << ")\n";
    }
    for (size_t i = 0; i < cfg.maxStocks.abs_cap_by_id.size(); ++i) {
        const auto &item_name = cfg.id_to_item[i];
        int cap = cfg.maxStocks.abs_cap_by_id[i];
        double factor = cfg.maxStocks.factor_by_id[i];
        if (cap < 0 && factor < 0.0) {
            continue; // No cap or factor for this item
        }
        std::cout<< item_name << '\n';
        if (cap < 0) {
            std::cout << "    No Cap\n";
        } else {
            std::cout << "    Cap: " << cap << '\n';
        }
        if (factor >= 0.0) {
            std::cout << "    Factor: " << factor << '\n';
        } else {
            std::cout << "    No factor limit\n";
        }
    }
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

        print_max_stocks(cfg);

        Candidate best_candidate = solve_with_ga(cfg, 30000);
        std::cout << "Simulation trace:\n";
        for (const auto &entry : best_candidate.trace) {
            std::cout << "Cycle " << entry.cycle << ": Start process " << cfg.processes[entry.procId].name << '\n';
        }

        std::cout << '\n';
        std::cout << "Total cycles:" << best_candidate.cycle << "\n";

        std::cout << '\n';
        std::cout << "Final stocks:\n";
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