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

#endif
