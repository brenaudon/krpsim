/*!
 *  @file parsing.hpp
 *  @brief Header file for parsing configuration data
 *
 *  This file defines structures and functions for parsing configuration data
 *  for a resource management system.
 */


#ifndef PARSING_HPP
#define PARSING_HPP

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

///< @brief Represents an item for a process
struct Item {
    std::string    name;    ///< Name of the item.
    int            qty;     ///< Quantity of the item needed or produced.
};

///< @brief Represents a process in the resource management system.
struct Process {
    std::string          name;      ///< Name of the process.
    std::vector<Item>    needs;     ///< Items required for the process, each with a name and quantity.
    std::vector<Item>    results;   ///< Items produced by the process, each with a name and quantity.
    int                  delay;     ///< Delay in cycles for the process to complete.
};

///< @brief Configuration structure for the resource management system.
struct Config {
    std::unordered_map<std::string, int>    initialStocks;  ///< Initial stock of items, keyed by item name.
    std::vector<Process>                    processes;      ///< List of processes, each with its name, needs, results, and delay.
    std::vector<std::string>                optimizeKeys;   ///< List of keys to optimize, typically process names.
};

/**
 * @brief Parse the configuration from an input stream.
 *
 * This function reads a configuration from the provided input stream,
 * and populates a Config struct with the parsed data. It expects the configuration
 * to be in a specific format, with sections for stocks, processes, and optimization keys.
 *
 * @param in The input stream to read the configuration from.
 * @return A Config struct containing the parsed configuration.
 * @throws std::runtime_error if the configuration is malformed.
 */
Config parse_config(std::istream &in);

#endif
