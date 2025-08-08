/*!
 *  @file krpsim.hpp
 *  @brief Header file for the krpsim resource management simulation
 *
 *  This file defines the structures used in the krpsim resource management simulation,
 *  including items, processes, and configuration settings.
 *
 */

#ifndef KRPSIM_HPP
#define KRPSIM_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>

///< @brief Represents an item for a process
struct Item {
    std::string    name;    ///< Name of the item.
    int            qty;     ///< Quantity of the item needed or produced.

    bool operator==(const Item& other) const noexcept {
        return name == other.name;
    }
};

///< @brief Represents a process in the resource management system.
struct Process {
    std::string          name;      ///< Name of the process.
    std::vector<Item>    needs;     ///< Items required for the process, each with a name and quantity.
    std::vector<Item>    results;   ///< Items produced by the process, each with a name and quantity.
    int                  delay;     ///< Delay in cycles for the process to complete.
    bool                 in_cycle{};  ///< Whether the process is in an obvious cycle.
};

///< @brief Configuration structure for the resource management system.
struct Config {
    std::unordered_map<std::string, int>    initialStocks;  ///< Initial stock of items, keyed by item name.
    std::vector<Process>                    processes;      ///< List of processes, each with its name, needs, results, and delay.
    std::vector<std::string>                optimizeKeys;   ///< List of keys to optimize, typically process names.
    std::unordered_map<std::string, double> dist;           ///< Distance of each stock item from the goal, keyed by item name.
};

#endif
