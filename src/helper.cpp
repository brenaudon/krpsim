/*!
 *  @file helper.cpp
 *  @brief Helper functions for configuration parsing and output.
 *
 * This file contains utility functions for trimming strings and printing
 * configuration details.
 */

#include "helper.hpp"

std::string ltrim(std::string s) {
    auto it = std::find_if_not(s.begin(), s.end(), ::isspace);
    return {it, s.end()};
}

std::string rtrim(std::string s) {
    auto it = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    return {s.begin(), it};
}

std::string trim(std::string s) {
    return rtrim(ltrim(s));
}

void print_config(const Config &cfg) {
    std::cout << "Stocks (" << cfg.initialStocks.size() << ")\n";
    for (auto &[k, v] : cfg.initialStocks) std::cout << "  - " << k << " : " << v << '\n';
    std::cout << "\nProcesses (" << cfg.processes.size() << ")\n";
    for (auto &p : cfg.processes) {
        std::cout << "  - " << p.name << " (delay " << p.delay << ")\n    needs  : ";
        for (auto &i : p.needs)   std::cout << i.name << ':' << i.qty << ' ';
        std::cout << "\n    results: ";
        for (auto &i : p.results) std::cout << i.name << ':' << i.qty << ' ';
        std::cout << "\n";
    }
    std::cout << "\nOptimize: ";
    for (auto &k : cfg.optimizeKeys) std::cout << k << ' ';
    std::cout << '\n';
}