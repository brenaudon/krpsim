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
    std::string         name;       ///< Name of the process.
    std::vector<Item>   needs;      ///< Items required for the process, each with a name and quantity.
    std::vector<Item>   results;    ///< Items produced by the process, each with a name and quantity.
    int                 delay;      ///< Delay in cycles for the process to complete.
    bool                in_cycle{}; ///< Whether the process is in an obvious cycle.

    std::vector<std::pair<int,int>> needs_by_id;    ///< Needs of the process, each pair contains item ID and quantity.
    std::vector<std::pair<int,int>> results_by_id;  ///< Results of the process, each pair contains item ID and quantity.
};

struct MaxStocks {
    std::string                             limiting_item{};          ///< Item that limits the maximum stock.
    int                                     limiting_initial_stock{}; ///< Initial stock of the limiting item. If -1, means we have to use factor to calculate max stock from current limiting item stock at each process choice.
    std::vector<int>                        abs_cap_by_id;            ///< Absolute cap for each item by ID, -1 means no cap.
    std::vector<double>                     factor_by_id;             ///< Factor to calculate max stock from current limiting item stock at each process choice, keyed by item ID. If -1.0, means no limit on the item.
};

///< @brief Configuration structure for the resource management system.
struct Config {
    std::unordered_map<std::string, int>    initialStocks;  ///< Initial stock of items, keyed by item name.
    std::vector<Process>                    processes;      ///< List of processes, each with its name, needs, results, and delay.
    std::vector<std::string>                optimizeKeys;   ///< List of keys to optimize, typically process names.
    std::unordered_map<std::string, double> dist;           ///< Distance of each stock item from the goal, keyed by item name.
    MaxStocks                               maxStocks;      ///< Structure used to know the maximum stock for each item.

    std::unordered_map<std::string,int>     item_to_id;     ///< Mapping from item name to its ID, used for quick access.
    std::vector<std::string>                id_to_item;     ///< Mapping from item ID to its name, used for quick access.

    std::vector<std::vector<std::pair<int,int>>>    needers_by_item;   ///< List of processes that need each item, each pair contains process ID and quantity needed ([item_id] -> {(pid, qty), ...})
};

#endif
