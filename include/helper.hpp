/*!
 *  @file helper.hpp
 *  @brief Helper functions for configuration parsing and output.
 *
 * This file contains utility functions for trimming strings and printing
 * configuration details.
 */

#ifndef HELPER_H
#define HELPER_H

#include "parsing.hpp"
#include <string>

std::string ltrim(std::string s); ///< @brief Left trim a string
std::string rtrim(std::string s); ///< @brief Right trim a string
std::string trim(std::string s); ///< @brief Trim a string (both sides)

/**
 * @brief Print the configuration to stdout.
 *
 * This function prints the configuration details including stocks, processes,
 * optimize keys, and their respective quantities and delays.
 *
 * @param cfg The configuration to print.
 */
void print_config(const Config &cfg);


#endif
