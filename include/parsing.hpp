/*!
 *  @file parsing.hpp
 *  @brief Header file for parsing configuration data
 *
 *  This file defines structures and functions for parsing configuration data
 *  for a resource management system.
 */

#ifndef PARSING_HPP
#define PARSING_HPP

#include <filesystem>
#include <charconv>
#include <regex>
#include <queue>
#include <unordered_set>

#include "krpsim.hpp"

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

/**
 * @brief Parse the configuration for simulation purposes.
 *
 * This function parses the configuration from an input stream, initializes the distance map for optimization keys,
 * selects necessary processes, builds item indices and IDs, and prepares the needers_by_item vector.
 *
 * @param in The input stream to read the configuration from.
 * @return A Config object containing the parsed and prepared configuration for simulation.
 */
Config parse_config_for_simulation(std::istream &in);

#endif
