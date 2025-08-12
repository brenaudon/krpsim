/*!
 *  @file parsing.cpp
 *  @brief Implementation of the configuration parsing functionality.
 *
 *  This file contains the implementation of functions to parse a configuration
 *  file for a resource management system.
 */


#include "helper.hpp"
#include "parsing.hpp"

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


/**
 * @brief Build a distance map for the given goal item, recursively updating the distances of its needs.
 *
 * @param goal The name of the goal item.
 * @param depth The current depth in the recursion, used to calculate distances.
 * @param cfg The configuration containing processes and their needs/results.
 */
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


/**
 * @brief Detect obvious cycles in the processes by checking if a process can lead back to itself.
 *
 * This function iterates through the processes and checks if any process can lead back to itself
 * through its results and needs, marking them as being in a cycle.
 * We consider a cycle obvious if each process produce exactly the items needed by the next process until the first one is reached again.
 *
 *
 * @param cfg The configuration containing the processes to check for cycles.
 */
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
            while (temp_it != cfg.processes.end() && cycle_starts_from.count(temp_it->name)) {
                ++temp_it;
            }
            it = temp_it;
            if (it == cfg.processes.end()) {
                break; // No more processes to check
            }
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


/**
 * @brief Recursively select processes that produce the target item and their dependencies.
 *
 * This function traverses the processes in the configuration and selects those that produce
 * the target item, recursively including all processes that are needed by them.
 *
 * @param cfg The configuration containing the processes.
 * @param selected_processes A set to keep track of selected process names.
 * @param target The target item to search for in the results of the processes.
 */
void select_processes_rec(Config &cfg, std::unordered_set<std::string>& selected_processes, const Item& target) {
    for (const Process &proc : cfg.processes) {
        if (std::find(proc.results.begin(), proc.results.end(), target) != proc.results.end()) {
            if (selected_processes.find(proc.name) == selected_processes.end()) {
                selected_processes.insert(proc.name);
                for (const Item &need : proc.needs) {
                    select_processes_rec(cfg, selected_processes, need);
                }
            }
        }
    }
}


/**
 * @brief Select processes based on the optimization keys in the configuration.
 *
 * This function iterates through the optimization keys and selects processes that are needed
 * to produce the items specified in those keys, excluding the "time" key.
 *
 * @param cfg The configuration containing the processes and optimization keys.
 */
void processes_selection(Config &cfg) {
    std::unordered_set<std::string> selected_processes;
    for (const auto &goal : cfg.optimizeKeys) {
        if (goal != "time") {
            Item target{goal, 0};
            select_processes_rec(cfg, selected_processes, target);
        }
    }

    // Remove processes not in the selection
    std::vector<Process> filtered_processes;
    for (const Process &proc : cfg.processes) {
        if (selected_processes.find(proc.name) != selected_processes.end()) {
            filtered_processes.push_back(proc);
        }
    }
    if (!filtered_processes.empty()) {
        cfg.processes = std::move(filtered_processes);
    }
}


/**
 * @brief Recursively calculate the maximum stocks needed for each item.
 *
 * This function updates the needed and produced stocks for the target item and its dependencies.
 *
 * @param cfg The configuration containing processes and their needs/results.
 * @param explored_processes A set to keep track of already explored processes to avoid cycles.
 * @param needed_stocks A map to keep track of the total needed stocks for each item.
 * @param produced_stocks A map to keep track of the total produced stocks for each item.
 * @param target The target item to calculate stocks for.
 */
void max_stocks_rec(Config &cfg, std::unordered_set<std::string>& explored_processes,
                    std::unordered_map<std::string, int> &needed_stocks,
                    std::unordered_map<std::string, int> &produced_stocks, const Item &target) {
    for (const Process &proc : cfg.processes) {
        if (std::find(proc.results.begin(), proc.results.end(), target) != proc.results.end()) {
            if (explored_processes.find(proc.name) == explored_processes.end()) {
                explored_processes.insert(proc.name);
                for (const Item &need : proc.needs) {
                    if (needed_stocks.find(need.name) == needed_stocks.end()) {
                        needed_stocks[need.name] = 0;
                    }
                    needed_stocks[need.name] += need.qty;
                    max_stocks_rec(cfg, explored_processes, needed_stocks, produced_stocks, need);
                }
                for (const Item &result : proc.results) {
                    if (produced_stocks.find(result.name) == produced_stocks.end()) {
                        produced_stocks[result.name] = 0;
                    }
                    produced_stocks[result.name] += result.qty;
                }
            }
        }
    }
}


/**
 * @brief Build the maximum stocks for each item based on the optimization keys.
 *
 * This function calculates the maximum stocks for each item based on the processes and their dependencies,
 * and sets the limiting item and its initial stock in the configuration.
 *
 * @param cfg The configuration containing processes, initial stocks, and optimization keys.
 */
void build_max_stocks(Config &cfg) {
    std::unordered_map<std::string, int> needed_stocks;
    std::unordered_map<std::string, int> produced_stocks;
    std::unordered_set<std::string> explored_processes;
    for (const auto &goal : cfg.optimizeKeys) {
        if (goal != "time") {
            Item target{goal, 0};
            max_stocks_rec(cfg, explored_processes, needed_stocks, produced_stocks, target);
        }
    }

    std::unordered_map<std::string, int> final_stocks;
    for (const auto& pair: needed_stocks) {
        int needed_stock = pair.second;
        int produced_stock = 0;
        if (produced_stocks.find(pair.first) != produced_stocks.end())
            produced_stock = produced_stocks[pair.first];
        final_stocks[pair.first] = produced_stock - needed_stock;
    }
    // Add stocks that are only produced
    for (const auto& pair : produced_stocks) {
        if (final_stocks.find(pair.first) == final_stocks.end()) {
            final_stocks[pair.first] = pair.second;
        }
    }

    // find min stock in final_stocks
    int min_stock = std::numeric_limits<int>::max();
    std::string min_stock_name;
    for (const auto& pair : final_stocks) {
        if (pair.second >= 0 && pair.second < min_stock) {
            if (pair.second == 0 && cfg.initialStocks.find(pair.first) == cfg.initialStocks.end()) {
                // If the stock is 0 but not in initial stocks, we cannot use it as a limiting item
                continue;
            }
            min_stock_name = pair.first;
            min_stock = pair.second;
        }
    }

    cfg.maxStocks.limiting_item = min_stock_name;
    std::unordered_map<std::string, int> max_stocks{}; // Maximum stock for each item, keyed by item name.
    std::unordered_map<std::string, double> max_stocks_factors{}; // Factors to calculate max stock from current limiting item stock at each process choice, keyed by item name. If -1.0, means no limit on the item.

    // If min_stock (limiting item) is 0, means as much is produced as needed, so we can only use the initial stock
    if (min_stock == 0) {
        int init_limiting_stock = cfg.initialStocks[min_stock_name];
        cfg.maxStocks.limiting_initial_stock = init_limiting_stock;
        for (const auto& pair : final_stocks) {
            if (pair.first == min_stock_name) {
                max_stocks[pair.first] = init_limiting_stock;
            } else {
                max_stocks[pair.first] = needed_stocks[pair.first] * (init_limiting_stock / needed_stocks[min_stock_name]);
            }
        }
    } else {
        // If min_stock is greater than 0, we keep a factor to calculate max_stock from the limiting current stock at each process choice
        cfg.maxStocks.limiting_initial_stock = -1; // -1 means we have to use factor to calculate max stock from current limiting item stock at each process choice
        for (const auto& pair : final_stocks) {
            max_stocks_factors[pair.first] = static_cast<double>(pair.second) / min_stock;
            if (pair.first == min_stock_name) {
                max_stocks_factors[pair.first] = -1.0; // No limit on the limiting item
            }
        }
    }

    // No limit on optimized items
    for (const auto& goal : cfg.optimizeKeys) {
        if (goal != "time") {
            max_stocks[goal] = std::numeric_limits<int>::max();
            max_stocks_factors[goal] = -1.0; // No factor needed for optimized items
        }
    }


    const int N = static_cast<int>(cfg.item_to_id.size());
    cfg.maxStocks.abs_cap_by_id.assign(N, -1);
    cfg.maxStocks.factor_by_id.assign(N, -1.0);
    for (auto& [name, cap] : max_stocks)
        cfg.maxStocks.abs_cap_by_id[cfg.item_to_id[name]] = cap;
    for (auto& [name, fac] : max_stocks_factors)
        cfg.maxStocks.factor_by_id[cfg.item_to_id[name]] = fac;

}

static int get_or_make_id(const std::string& s,
                          std::unordered_map<std::string,int>& map,
                          std::vector<std::string>& vec) {
    auto it = map.find(s);
    if (it != map.end()) return it->second;
    int id = static_cast<int>(vec.size());
    map.emplace(s, id);
    vec.push_back(s);
    return id;
}

void build_item_index_and_ids(Config& cfg) {
    cfg.item_to_id.clear();
    cfg.id_to_item.clear();

    // Collect all item names from stocks and processes
    for (auto& [name, _] : cfg.initialStocks)
        get_or_make_id(name, cfg.item_to_id, cfg.id_to_item);
    for (auto& p : cfg.processes) {
        for (auto& it : p.needs)
            get_or_make_id(it.name, cfg.item_to_id, cfg.id_to_item);
        for (auto& it : p.results)
            get_or_make_id(it.name, cfg.item_to_id, cfg.id_to_item);
    }

    // Fill ID-based vectors on processes
    for (auto& p : cfg.processes) {
        p.needs_by_id.clear();
        p.needs_by_id.reserve(p.needs.size());
        p.results_by_id.clear();
        p.results_by_id.reserve(p.results.size());
        for (auto& it : p.needs)
            p.needs_by_id.emplace_back(cfg.item_to_id[it.name], it.qty);
        for (auto& it : p.results)
            p.results_by_id.emplace_back(cfg.item_to_id[it.name], it.qty);
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

    // check if several processes have the same name
    std::unordered_set<std::string> process_names;
    for (const Process &proc : cfg.processes) {
        if (process_names.find(proc.name) != process_names.end()) {
            throw std::runtime_error("Duplicate process name: '" + proc.name + "'");
        }
        process_names.insert(proc.name);
    }

    // Initialize the distance map for optimization keys
    for (const std::string& goal : cfg.optimizeKeys) {
        if (goal != "time") {
            cfg.dist[goal] = 0.0;
            build_dist_map(goal, 0.0, cfg);
            break;
        }
    }

    // // TODO: check if need to be removed
    // for (const std::string& goal : cfg.optimizeKeys) {
    //     if (goal != "time") {
    //         for (Process& proc : cfg.processes) {
    //             for (const Item& item : proc.needs) {
    //                 if (item.name == goal) {
    //                     proc.use_optimized_items = true;
    //                 }
    //             }
    //         }
    //     }
    // }

    // Remove processes that are not needed for production of the optimization keys
    if (cfg.optimizeKeys.size() != 1 || cfg.optimizeKeys[0] != "time") {
        processes_selection(cfg);
    }

    build_item_index_and_ids(cfg);

    // Build the max_stock map representing the maximum stock for each item
    if (cfg.optimizeKeys.size() != 1 || cfg.optimizeKeys[0] != "time") {
        build_max_stocks(cfg);
    }

    // Detect obvious cycles in the processes
    detect_obvious_cycles(cfg);

    return cfg;
}
