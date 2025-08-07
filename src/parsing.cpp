/*!
 *  @file parsing.cpp
 *  @brief Implementation of the configuration parsing functionality.
 *
 *  This file contains the implementation of functions to parse a configuration
 *  file for a resource management system.
 */


#include "helper.hpp"
#include "parsing.hpp"

#include <unordered_set>

/**
 * @brief Parse a single item from a string in the format "name:qty".
 *
 * @param token The string to parse.
 * @return An Item struct containing the name and quantity.
 * @throws std::runtime_error if the format is incorrect.
 */
static Item parse_item(std::string token) {
    auto colon = token.find(':');
    if (colon == std::string::npos)
        throw std::runtime_error("Bad item (no colon): '" + std::string(token) + "'");

    std::string name_sv = trim(token.substr(0, colon));
    if (name_sv.empty())
        throw std::runtime_error("Bad item (empty name): '" + std::string(token) + "'");

    std::string qty_sv = trim(token.substr(colon + 1));
    int qty = 0;
    auto [ptr, ec] = std::from_chars(qty_sv.data(), qty_sv.data() + qty_sv.size(), qty);
    if (ec != std::errc{})
        throw std::runtime_error("Bad item (qty): '" + std::string(token) + "'");

    return {std::string(name_sv), qty};
}

/**
 * @brief Parse a list of items from a string, where items are separated by semicolons.
 *
 * @param list The string containing the list of items.
 * @return A vector of Item structs parsed from the string.
 */
static std::vector<Item> parse_item_list(std::string list) {
    std::vector<Item> items;
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(';', start);
        std::string tok = (end == std::string::npos) ? list.substr(start) : list.substr(start, end - start);
        tok = trim(tok);
        if (!tok.empty()) items.push_back(parse_item(tok));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return items;
}

///< @brief Namespace for regex patterns used in parsing.
namespace detail {
    const std::regex re_stock(R"(^\s*([^:#\s]+)\s*:\s*(\d+)\s*$)");
    const std::regex re_process(R"(^\s*([^:]+?)\s*:\s*\(([^)]*)\)\s*:\s*(?:\(([^)]*)\))?\s*:\s*(\d+)\s*$)");
    const std::regex re_optimize(R"(^\s*optimize\s*:\s*\(([^)]*)\)\s*$)", std::regex::icase);
}

void build_dist_map(const std::string goal, double depth, Config& cfg) {
    for (const Process& proc : cfg.processes) {
        for (const Item& item : proc.results) {
            if (item.name == goal) {
                bool deadend = true;
                for (const Item& need : proc.needs) {
                    if (cfg.dist.find(need.name) == cfg.dist.end()) {
                        cfg.dist[need.name] = depth + 1.0;
                        deadend = false;
                    }
                    if (!deadend) {
                        build_dist_map(need.name, depth + 1, cfg);
                    }
                }
            }
        }
    }
}

void detect_obvious_cycles(Config &cfg) {
    std::vector<std::string> cycle_names;
    std::unordered_map<std::string, bool> cycle_starts_from;

    std::vector<Process>::iterator it = cfg.processes.begin();
    while (it != cfg.processes.end()) {
        Process proc = *it;
        if (proc.results.empty()) {
            ++it;
            continue; // Skip processes that produce nothing
        }
        bool in_cycle = false;

        // Back to cycle start
        if (!cycle_names.empty() && cycle_names[0] == proc.name) {
            std::cout << "\n";
            for (const auto& name : cycle_names) {
                for (Process& p : cfg.processes) {
                    if (p.name == name) {
                        p.in_cycle = true;
                    }
                }
            }
        }
        // If the process is already in a cycle, cycle is done so clear and skip
        if (std::find(cycle_names.begin(), cycle_names.end(), proc.name) != cycle_names.end()) {
            std::vector<Process>::iterator temp_it = cfg.processes.begin();
            while (temp_it != cfg.processes.end()) {
                if (temp_it->name == cycle_names[0]) {
                    break;
                }
                ++temp_it;
            }
            ++temp_it;
            while (cycle_starts_from[temp_it->name] && temp_it != cfg.processes.end()) {
                ++temp_it;
            }
            it = temp_it;
            cycle_starts_from[temp_it->name] = true;
            cycle_names.clear();
            continue;
        }
        ++it;

        std::vector<Process>::iterator sec_it = cfg.processes.begin();
        Process sec_proc;
        bool clear_cycles = true;
        while (sec_it != cfg.processes.end()) {
            sec_proc = *sec_it;
            if (proc.results.size() == sec_proc.needs.size()) {
                for (Item& item : proc.results) {
                    if (std::find(sec_proc.needs.begin(), sec_proc.needs.end(), item) == sec_proc.needs.end()) {
                        break;
                    }
                    if (item.name == proc.results[proc.results.size() - 1].name) {
                        in_cycle = true;
                    }
                }
                if (in_cycle) {
                    cycle_names.push_back(proc.name);
                    if (!cycle_starts_from[proc.name]) {
                        cycle_starts_from[proc.name] = true;
                    }
                    it = sec_it;
                    clear_cycles = false;
                    break;
                }
            }
            ++sec_it;
        }
        if (clear_cycles) {
            cycle_names.clear();
        }
    }
}


Config parse_config(std::istream &in) {
    Config cfg;
    std::string line;
    size_t lineno = 0;

    enum class Section { STOCKS, PROCESSES, OPTIMIZE } section = Section::STOCKS;

    while (std::getline(in, line)) {
        ++lineno;
        std::string trimmed(line);
        trimmed = std::string(trim(trimmed));
        if (trimmed.empty() || trimmed.front() == '#') continue;

        std::smatch m;
        switch (section) {
            case Section::STOCKS:
                if (std::regex_match(trimmed, m, detail::re_stock)) {
                    cfg.initialStocks.emplace(m[1].str(), std::stoll(m[2].str()));
                    break;
                } else if (std::regex_match(trimmed, m, detail::re_process)) {
                    section = Section::PROCESSES; // fall‑through to parse as process
                } else {
                    throw std::runtime_error("Expected stock or process at line " + std::to_string(lineno));
                }
            case Section::PROCESSES:
                if (std::regex_match(trimmed, m, detail::re_process)) {
                    Process p;
                    p.name = trim(m[1].str());
                    p.needs = parse_item_list(m[2].str());
                    p.results = parse_item_list(m[3].str());
                    p.delay = std::stoll(m[4].str());
                    cfg.processes.emplace_back(std::move(p));
                    break;
                } else if (std::regex_match(trimmed, m, detail::re_optimize)) {
                    section = Section::OPTIMIZE;
                } else {
                    throw std::runtime_error("Expected process or optimize at line " + std::to_string(lineno));
                }
            case Section::OPTIMIZE:
                if (std::regex_match(trimmed, m, detail::re_optimize)) {
                    std::string inside = m[1];
                    size_t start = 0;
                    while (start < inside.size()) {
                        size_t end = inside.find(';', start);
                        std::string tok{trim(inside.substr(start, end - start))};
                        if (!tok.empty()) cfg.optimizeKeys.push_back(tok);
                        if (end == std::string::npos) break;
                        start = end + 1;
                    }
                } else {
                    throw std::runtime_error("Unexpected content after optimize at line " + std::to_string(lineno));
                }
                break;
        }
    }

    if (cfg.optimizeKeys.empty())
        throw std::runtime_error("Missing optimize section");

    for (const std::string& goal : cfg.optimizeKeys) {
        if (goal != "time") {
            cfg.dist[goal] = 0.0;
            build_dist_map(goal, 0.0, cfg);
            break;
        }
    }

    // Remove processes producing nothing
    std::vector<Process> processes_copy = cfg.processes;
    for (const Process &proc : processes_copy) {
        if (proc.results.empty()) {
            auto it = std::remove_if(cfg.processes.begin(), cfg.processes.end(),
                                     [&proc](const Process &p) { return p.name == proc.name; });
            cfg.processes.erase(it, cfg.processes.end());
        }
    }

    detect_obvious_cycles(cfg);

    return cfg;
}
